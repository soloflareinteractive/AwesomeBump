/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "glimageeditor.h"



GLImage::GLImage(QWidget *parent)
    : QGLWidget(QGLFormat::defaultFormat(), parent)

{
    bShadowRender         = false;
    bSkipProcessing       = false;
    conversionType        = CONVERT_NONE;
    uvManilupationMethod  = UV_TRANSLATE;
    cornerWeights         = QVector4D(0,0,0,0);
    fboRatio = 1;

    // initialize position of the corners
    cornerPositions[0] = QVector2D(-0.0,-0);
    cornerPositions[1] = QVector2D( 1,-0);
    cornerPositions[2] = QVector2D( 1, 1);
    cornerPositions[3] = QVector2D(-0, 1);
    draggingCorner       = -1;
    gui_perspective_mode =  0;
    gui_seamless_mode    =  0;
    setCursor(Qt::OpenHandCursor);
    cornerCursors[0] = QCursor(QPixmap(":/resources/corner1.png"));
    cornerCursors[1] = QCursor(QPixmap(":/resources/corner2.png"));
    cornerCursors[2] = QCursor(QPixmap(":/resources/corner3.png"));
    cornerCursors[3] = QCursor(QPixmap(":/resources/corner4.png"));
    setMouseTracking(true);
}

GLImage::~GLImage()
{
  cleanup();
}

void GLImage::cleanup()
{
  makeCurrent();
  
  glDeleteBuffers(sizeof(vbos)/sizeof(GLuint), &vbos[0]);
  delete program;

  doneCurrent();
}

QSize GLImage::minimumSizeHint() const
{
    return QSize(360, 360);

}

QSize GLImage::sizeHint() const
{
    return QSize(500, 400);
}

void GLImage::initializeGL()
{

    initializeOpenGLFunctions();

    qDebug() << "calling " << Q_FUNC_INFO;
    
    QColor clearColor = QColor::fromCmykF(0.79, 0.79, 0.79, 0.0).dark();
    GLCHK( glClearColor((GLfloat)clearColor.red() / 255.0, (GLfloat)clearColor.green() / 255.0,
			(GLfloat)clearColor.blue() / 255.0, (GLfloat)clearColor.alpha() / 255.0) );
    GLCHK( glEnable(GL_MULTISAMPLE) );
    GLCHK( glEnable(GL_DEPTH_TEST) );
    // glEnable(GL_TEXTURE_2D); // non-core


    qDebug() << "Loading filters (fragment shader)";
    QOpenGLShader *vshader = new QOpenGLShader(QOpenGLShader::Vertex, this);
    vshader->compileSourceFile(":/resources/filters.vert");
    if (!vshader->log().isEmpty()) qDebug() << vshader->log();
    else qDebug() << "done";

    qDebug() << "Loading filters (vertex shader)";
    QOpenGLShader *fshader = new QOpenGLShader(QOpenGLShader::Fragment, this);
    fshader->compileSourceFile(":/resources/filters.frag");
    if (!fshader->log().isEmpty()) qDebug() << fshader->log();
    else qDebug() << "done";

    program = new QOpenGLShaderProgram(this);
    program->addShader(vshader);
    program->addShader(fshader);
    program->bindAttributeLocation("positionIn", 0);
    GLCHK( program->link() );

    GLCHK( program->bind() );
    GLCHK( program->setUniformValue("layerA" , 0) );
    GLCHK( program->setUniformValue("layerB" , 1) );
    GLCHK( program->setUniformValue("layerC" , 2) );

    delete vshader;
    delete fshader;

    makeScreenQuad();
    GLCHK( subroutines["mode_normal_filter"]               = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_normal_filter") );
    GLCHK( subroutines["mode_color_hue_filter"]               = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_color_hue_filter") );

    GLCHK( subroutines["mode_overlay_filter"]              = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_overlay_filter") );
    GLCHK( subroutines["mode_ao_cancellation_filter"]      = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_ao_cancellation_filter") );


    GLCHK( subroutines["mode_invert_filter"]               = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_invert_filter") );
    GLCHK( subroutines["mode_gauss_filter"]                = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_gauss_filter") );
    GLCHK( subroutines["mode_seamless_filter"]             = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_seamless_filter") );
    GLCHK( subroutines["mode_dgaussians_filter"]           = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_dgaussians_filter") );
    GLCHK( subroutines["mode_constrast_filter"]            = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_constrast_filter") );
    GLCHK( subroutines["mode_small_details_filter"]        = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_small_details_filter") );
    GLCHK( subroutines["mode_gray_scale_filter"]           = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_gray_scale_filter") );
    GLCHK( subroutines["mode_medium_details_filter"]       = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_medium_details_filter") );
    GLCHK( subroutines["mode_height_to_normal"]            = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_height_to_normal") );
    GLCHK( subroutines["mode_sharpen_blur"]                = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_sharpen_blur") );
    GLCHK( subroutines["mode_normals_step_filter"]         = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_normals_step_filter") );
    GLCHK( subroutines["mode_invert_components_filter"]    = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_invert_components_filter") );
    GLCHK( subroutines["mode_normal_to_height"]            = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_normal_to_height") );
    GLCHK( subroutines["mode_sobel_filter"]                = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_sobel_filter") );
    GLCHK( subroutines["mode_normal_expansion_filter"]     = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_normal_expansion_filter") );
    GLCHK( subroutines["mode_normalize_filter"]            = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_normalize_filter") );
    GLCHK( subroutines["mode_smooth_filter"]               = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_smooth_filter") );
    GLCHK( subroutines["mode_occlusion_filter"]            = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_occlusion_filter") );
    GLCHK( subroutines["mode_combine_normal_height_filter"]= glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_combine_normal_height_filter") );
    GLCHK( subroutines["mode_perspective_transform_filter"]= glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_perspective_transform_filter") );
    GLCHK( subroutines["mode_height_processing_filter"]    = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_height_processing_filter" ) );
    GLCHK( subroutines["mode_roughness_filter"]            = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_roughness_filter" ) );
    GLCHK( subroutines["mode_roughness_color_filter"]      = glGetSubroutineIndex(program->programId(),GL_FRAGMENT_SHADER,"mode_roughness_color_filter" ) );

    emit readyGL();
}

void GLImage::paintGL()
{
    render();
    emit rendered();
}



void GLImage::render(){

    if (!activeImage) return;
  
    // do not clear the background during rendering process
    if(!bShadowRender)
        GLCHK( glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) );
    GLCHK( glDisable(GL_CULL_FACE) );
    GLCHK( glDisable(GL_DEPTH_TEST) );

    // positions
    glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(float)*3,(void*)0);
    // indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos[2]);

    QGLFramebufferObject* activeFBO     = activeImage->fbo;
    QGLFramebufferObject* activeAuxFBO  = activeImage->aux_fbo;
    QGLFramebufferObject* activeAux2FBO = activeImage->aux2_fbo;

    bool bTransformUVs = true; // images which depend on others will not be affected by UV changes again
    bool bSkipStandardProcessing = false;

    if(!bSkipProcessing == true){
    // resizing the FBOs in case of convertion procedure
    switch(conversionType){
        case(CONVERT_FROM_H_TO_N):
        /*
        if(activeImage->imageType == NORMAL_TEXTURE){
            FBOImages::resize(targetImageNormal->ref_fbo  ,activeImage->ref_fbo);
            FBOImages::resize(targetImageNormal->fbo      ,activeImage->ref_fbo);
            FBOImages::resize(targetImageNormal->aux_fbo  ,activeImage->ref_fbo);
            FBOImages::resize(targetImageNormal->aux2_fbo ,activeImage->ref_fbo);
            activeFBO     = targetImageNormal->fbo;
            activeAuxFBO  = targetImageNormal->aux_fbo;
            activeAux2FBO = targetImageNormal->aux2_fbo;
        }
        */
        break;
        case(CONVERT_FROM_N_TO_H):
        /*
        FBOImages::resize(targetImageHeight->ref_fbo  ,activeImage->ref_fbo);
        FBOImages::resize(targetImageHeight->fbo      ,activeImage->ref_fbo);
        FBOImages::resize(targetImageHeight->aux_fbo  ,activeImage->ref_fbo);
        FBOImages::resize(targetImageHeight->aux2_fbo ,activeImage->ref_fbo);
        activeFBO     = targetImageHeight->fbo;
        activeAuxFBO  = targetImageHeight->aux_fbo;
        activeAux2FBO = targetImageHeight->aux2_fbo;
        */
        break;
        case(CONVERT_FROM_D_TO_O):
        /*
        FBOImages::resize(targetImageHeight->ref_fbo     ,activeImage->ref_fbo);
        FBOImages::resize(targetImageHeight->fbo         ,activeImage->ref_fbo);
        FBOImages::resize(targetImageHeight->aux_fbo     ,activeImage->ref_fbo);
        FBOImages::resize(targetImageHeight->aux2_fbo    ,activeImage->ref_fbo);

        FBOImages::resize(targetImageNormal->ref_fbo     ,activeImage->ref_fbo);
        FBOImages::resize(targetImageNormal->fbo         ,activeImage->ref_fbo);
        FBOImages::resize(targetImageNormal->aux_fbo     ,activeImage->ref_fbo);
        FBOImages::resize(targetImageNormal->aux2_fbo    ,activeImage->ref_fbo);

        FBOImages::resize(targetImageSpecular->ref_fbo   ,activeImage->ref_fbo);
        FBOImages::resize(targetImageSpecular->fbo       ,activeImage->ref_fbo);
        FBOImages::resize(targetImageSpecular->aux_fbo   ,activeImage->ref_fbo);
        FBOImages::resize(targetImageSpecular->aux2_fbo  ,activeImage->ref_fbo);

        FBOImages::resize(targetImageOcclusion->ref_fbo  ,activeImage->ref_fbo);
        FBOImages::resize(targetImageOcclusion->fbo      ,activeImage->ref_fbo);
        FBOImages::resize(targetImageOcclusion->aux_fbo  ,activeImage->ref_fbo);
        FBOImages::resize(targetImageOcclusion->aux2_fbo ,activeImage->ref_fbo);

        FBOImages::resize(targetImageRoughness->ref_fbo  ,activeImage->ref_fbo);
        FBOImages::resize(targetImageRoughness->fbo      ,activeImage->ref_fbo);
        FBOImages::resize(targetImageRoughness->aux_fbo  ,activeImage->ref_fbo);
        FBOImages::resize(targetImageRoughness->aux2_fbo ,activeImage->ref_fbo);

        FBOImages::resize(targetImageMetallic->ref_fbo  ,activeImage->ref_fbo);
        FBOImages::resize(targetImageMetallic->fbo      ,activeImage->ref_fbo);
        FBOImages::resize(targetImageMetallic->aux_fbo  ,activeImage->ref_fbo);
        FBOImages::resize(targetImageMetallic->aux2_fbo ,activeImage->ref_fbo);
        */
        break;
        case(CONVERT_RESIZE): // apply resize textures
            activeImage->resizeFBO(resize_width,resize_height);
            // pointers were changed in resize function
            activeFBO     = activeImage->fbo;
            activeAuxFBO  = activeImage->aux_fbo;
            activeAux2FBO = activeImage->aux2_fbo;
            bSkipStandardProcessing = true;
        break;
        default:
        break;
    }

    GLCHK( program->bind() );
    GLCHK( program->setUniformValue("gui_image_type", activeImage->imageType) );
    GLCHK( program->setUniformValue("gui_depth", float(1.0)) );
    GLCHK( program->setUniformValue("gui_mode_dgaussian", 1) );


    if(activeImage->bFirstDraw){
        resetView();
        qDebug() << "Doing first draw of" << PostfixNames::getTextureName(activeImage->imageType) << " texture.";

        activeImage->bFirstDraw = false;
        // Updating ref FBO...

        activeImage->ref_fbo->bind();
            glViewport(0,0,activeImage->ref_fbo->width(),activeImage->ref_fbo->height());
            program->setUniformValue("quad_scale", QVector2D(1.0,1.0));
            program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0));
            glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_normal_filter"]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, activeImage->scr_tex_id);
            glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0);
        activeImage->ref_fbo->bindDefault();
    }

    // First copy of reference Image to Active
    copyFBO(activeImage->ref_fbo,activeFBO);


    switch(activeImage->imageType){
        // ----------------------------------------------------
        //
        // ----------------------------------------------------
        case(NORMAL_TEXTURE):{
        // Choosing proper action

        switch(activeImage->inputImageType){
            case(INPUT_FROM_NORMAL_INPUT):
                if(conversionType == CONVERT_FROM_H_TO_N){
                    applyHeightToNormal(targetImageHeight->fbo,activeFBO);
                    bTransformUVs = false;
                }
                break;
            case(INPUT_FROM_HEIGHT_INPUT):
                // transform height before  normal calculation

                if(conversionType == CONVERT_NONE){
                    // perspective transformation treats differently normal texture
                    // change for a moment the texture type to performe transformations
                    GLCHK( program->setUniformValue("gui_image_type", HEIGHT_TEXTURE) );
                    copyFBO(targetImageHeight->ref_fbo,activeFBO);
                    applyPerspectiveTransformFilter(activeFBO,activeAuxFBO);// the output is saved to activeFBO

                    // Making seamless...
                    switch(FBOImageProporties::seamlessMode){
                        case(SEAMLESS_SIMPLE):
                        case(SEAMLESS_MIRROR):
                        case(SEAMLESS_RANDOM):
                            applySeamlessFilter(activeFBO,activeAuxFBO);
                        break;
                        case(SEAMLESS_NONE):
                        default: break;
                    }

                    applyHeightToNormal(activeAuxFBO,activeFBO);
                    GLCHK( program->setUniformValue("gui_image_type", activeImage->imageType) );
                    bTransformUVs = false;
                }else{
                    applyHeightToNormal(targetImageHeight->ref_fbo,activeFBO);
                }



                break;
            case(INPUT_FROM_HEIGHT_OUTPUT):
                applyHeightToNormal(targetImageHeight->fbo,activeFBO);
                bTransformUVs = false;
                break;
            default: break;
        }
        break;}// end of case Normal
        // ----------------------------------------------------
        //
        // ----------------------------------------------------
        case(SPECULAR_TEXTURE):{
        // Choosing proper action

        switch(activeImage->inputImageType){
            case(INPUT_FROM_SPECULAR_INPUT):
                // do nothing
                break;
            case(INPUT_FROM_HEIGHT_INPUT):
                copyFBO(targetImageHeight->ref_fbo,activeFBO);
                //bTransformUVs = false;
                break;
            case(INPUT_FROM_HEIGHT_OUTPUT):
                copyFBO(targetImageHeight->fbo,activeFBO);
                bTransformUVs = false;
                break;
            case(INPUT_FROM_DIFFUSE_INPUT):
                copyFBO(targetImageDiffuse->ref_fbo,activeFBO);
                //bTransformUVs = false;
                break;
            case(INPUT_FROM_DIFFUSE_OUTPUT):
                copyFBO(targetImageDiffuse->fbo,activeFBO);
                bTransformUVs = false;
                break;
            default: break;
        }
        break;}// end of case Specular
        // ----------------------------------------------------
        //
        // ----------------------------------------------------
        case(OCCLUSION_TEXTURE):{
        // Choosing proper action

        switch(activeImage->inputImageType){
            case(INPUT_FROM_OCCLUSION_INPUT):

                if(conversionType == CONVERT_FROM_HN_TO_OC){
                    // Ambient occlusion is calculated from normal and height map, so
                    // some part of processing is skiped
                    applyCombineNormalHeightFilter(targetImageNormal->fbo,targetImageHeight->fbo,activeAuxFBO);
                    applyOcclusionFilter(activeAuxFBO,activeFBO);
                    bSkipStandardProcessing =  true;
                    bTransformUVs = false;
                }

                break;
            case(INPUT_FROM_HI_NI):
                // Ambient occlusion is calculated from normal and height map, so
                // some part of processing is skiped
                applyCombineNormalHeightFilter(targetImageNormal->ref_fbo,targetImageHeight->ref_fbo,activeAuxFBO);
                applyOcclusionFilter(activeAuxFBO,activeFBO);

                break;     
            case(INPUT_FROM_HO_NO):
                applyCombineNormalHeightFilter(targetImageNormal->fbo,targetImageHeight->fbo,activeAuxFBO);
                applyOcclusionFilter(activeAuxFBO,activeFBO);
                bTransformUVs = false;
                break;
            default: break;
        }
        break;}// end of case Occlusion
        // ----------------------------------------------------
        //
        // ----------------------------------------------------
        case(HEIGHT_TEXTURE):{
        if(conversionType == CONVERT_FROM_N_TO_H){
            applyNormalToHeight(activeImage,targetImageNormal->fbo,activeFBO,activeAuxFBO);
            applyCPUNormalizationFilter(activeAuxFBO,activeFBO);
            applyGaussFilter(activeFBO,activeAuxFBO,activeAux2FBO,1,0.1); // small blur
            copyFBO(activeAux2FBO,activeFBO);
            copyFBO(activeFBO,targetImageHeight->ref_fbo);
            bTransformUVs = false;
        }
        // ----------------------------------------------------
        //
        // ----------------------------------------------------
        break;}// end of case Height
        case(ROUGHNESS_TEXTURE):{
        // Choosing proper action

        switch(activeImage->inputImageType){
            case(INPUT_FROM_ROUGHNESS_INPUT):
                // do nothing
                break;
            case(INPUT_FROM_HEIGHT_INPUT):
                copyFBO(targetImageHeight->ref_fbo,activeFBO);
                break;
            case(INPUT_FROM_HEIGHT_OUTPUT):
                copyFBO(targetImageHeight->fbo,activeFBO);
                bTransformUVs = false;
                break;
            case(INPUT_FROM_DIFFUSE_INPUT):
                copyFBO(targetImageDiffuse->ref_fbo,activeFBO);
                break;
            case(INPUT_FROM_DIFFUSE_OUTPUT):
                copyFBO(targetImageDiffuse->fbo,activeFBO);
                bTransformUVs = false;
                break;
            default: break;



        }

        break;
        }// end of case Roughness
        case(METALLIC_TEXTURE):{
        // Choosing proper action

        switch(activeImage->inputImageType){
            case(INPUT_FROM_METALLIC_INPUT):
                // do nothing
                break;
            case(INPUT_FROM_HEIGHT_INPUT):
                copyFBO(targetImageHeight->ref_fbo,activeFBO);
                break;
            case(INPUT_FROM_HEIGHT_OUTPUT):
                copyFBO(targetImageHeight->fbo,activeFBO);
                bTransformUVs = false;
                break;
            case(INPUT_FROM_DIFFUSE_INPUT):
                copyFBO(targetImageDiffuse->ref_fbo,activeFBO);
                break;
            case(INPUT_FROM_DIFFUSE_OUTPUT):
                copyFBO(targetImageDiffuse->fbo,activeFBO);
                bTransformUVs = false;
                break;
            default: break;
        }
        break;}// end of case Roughness
        default:break;

    };




    // Transform UVs in some cases

    if(conversionType == CONVERT_NONE && bTransformUVs){

        applyPerspectiveTransformFilter(activeFBO,activeAuxFBO);// the output is save to activeFBO

        // Making seamless...
        switch(FBOImageProporties::seamlessMode){
            case(SEAMLESS_SIMPLE):
            case(SEAMLESS_MIRROR):
            case(SEAMLESS_RANDOM):
                applySeamlessFilter(activeFBO,activeAuxFBO);
                copyFBO(activeAuxFBO,activeFBO);
            break;
            case(SEAMLESS_NONE):
            default: break;
        }
    }
    // skip all processing
    if(!bSkipStandardProcessing){

    // begin standart pipe-line (for each image)
    applyInvertComponentsFilter(activeFBO,activeAuxFBO);


    if(activeImage->imageType != HEIGHT_TEXTURE &&
       activeImage->imageType != NORMAL_TEXTURE &&
       activeImage->imageType != OCCLUSION_TEXTURE &&
       activeImage->imageType != ROUGHNESS_TEXTURE){

        // hue manipulation
        applyColorHueFilter(activeAuxFBO,activeFBO);
        copyFBO(activeFBO,activeAuxFBO);
    }

    // In case when color picking is enabled disable
    // gray scale filter
    if(!activeImage->bRoughnessEnableColorPicking){
    if(activeImage->bGrayScale ||
            activeImage->imageType == ROUGHNESS_TEXTURE ||
            activeImage->imageType == OCCLUSION_TEXTURE ||
            activeImage->imageType == HEIGHT_TEXTURE ){
        applyGrayScaleFilter(activeAuxFBO,activeFBO);
    }else{
        copyFBO(activeAuxFBO,activeFBO);
    }

    }else copyFBO(activeAuxFBO,activeFBO);


    // both metallic and roughness are almost the same
    // so use same filters for them
    if( (activeImage->imageType == ROUGHNESS_TEXTURE ||  activeImage->imageType == METALLIC_TEXTURE )
        && activeImage->bRoughnessSurfaceEnable ){
        // processing surface
        applyRoughnessFilter(activeFBO,activeAux2FBO,activeAuxFBO);
        copyFBO(activeAuxFBO,activeFBO);
    }



    // specular manipulation
    if(activeImage->bSpeclarControl && activeImage->imageType != HEIGHT_TEXTURE){
        applyDGaussiansFilter(activeFBO,activeAux2FBO,activeAuxFBO);
        applyContrastFilter(activeAuxFBO,activeFBO);
    }



    // -------------------------------------------------------- //
    // selective blur of height image
    // -------------------------------------------------------- //
    if(activeImage->bSelectiveBlurEnable){
        // mask input image
        switch(activeImage->selectiveBlurMaskInputImageType){
            case(INPUT_FROM_HEIGHT_OUTPUT):
                copyFBO(activeFBO,targetImageSpecular->aux2_fbo);
                break;
            case(INPUT_FROM_DIFFUSE_OUTPUT):
                copyFBO(targetImageDiffuse->fbo,targetImageSpecular->aux2_fbo);
                break;
            case(INPUT_FROM_ROUGHNESS_OUTPUT):
                copyFBO(targetImageRoughness->fbo,targetImageSpecular->aux2_fbo);
                break;
            case(INPUT_FROM_METALLIC_OUTPUT):
                copyFBO(targetImageMetallic->fbo,targetImageSpecular->aux2_fbo);
                break;
            default:
            break;
        }
        // apply filter for mask
        switch(activeImage->selectiveBlurType){
        case(SELECTIVE_BLUR_DIFFERENCE_OF_GAUSSIANS):
            applyDGaussiansFilter(targetImageSpecular->aux2_fbo,activeAux2FBO,activeAuxFBO,true);
            applyContrastFilter(activeAuxFBO,targetImageSpecular->aux_fbo,true);
            break;
        case(SELECTIVE_BLUR_LEVELS):
            applyHeightProcessingFilter(targetImageSpecular->aux2_fbo,targetImageSpecular->aux_fbo,true);
            break;
        };

        // blur image according to mask: targetImageSpecular->aux_fbo
        applyMaskedGaussFilter(activeFBO,targetImageSpecular->aux_fbo,targetImageSpecular->aux2_fbo,activeAux2FBO,activeAuxFBO);
        copyFBO(activeAuxFBO,activeFBO);

    } // end of selective blur

    // Removing shading...
    if(activeImage->bRemoveShading){
        applyGaussFilter(activeFBO,activeAux2FBO,activeAuxFBO,1);
        applyInverseColorFilter(activeAuxFBO,activeAux2FBO);
        copyFBO(activeAux2FBO,activeAuxFBO);
        applyOverlayFilter(activeFBO,activeAuxFBO,activeAux2FBO);

        applyRemoveShadingFilter(activeAux2FBO,
                                targetImageOcclusion->fbo,
                                activeFBO,
                                activeAuxFBO);

        copyFBO(activeAuxFBO,activeFBO);
    }

    if(activeImage->noBlurPasses > 0){
        for(int i = 0 ; i < activeImage->noBlurPasses ; i++ ){
            applyGaussFilter(activeFBO,activeAux2FBO,activeAuxFBO,1);
            applyOverlayFilter(activeFBO,activeAuxFBO,activeAux2FBO);
            copyFBO(activeAux2FBO,activeFBO);
        }
    }

    if( activeImage->smallDetails  > 0.0){
        applySmallDetailsFilter(activeFBO,activeAux2FBO,activeAuxFBO);
        copyFBO(activeAuxFBO,activeFBO);
    }


    if( activeImage->mediumDetails > 0.0){
        applyMediumDetailsFilter(activeFBO,activeAux2FBO,activeAuxFBO);
        copyFBO(activeAuxFBO,activeFBO);
    }

    if(activeImage->sharpenBlurAmount != 0){
        applySharpenBlurFilter(activeFBO,activeAux2FBO,activeAuxFBO);
        copyFBO(activeAuxFBO,activeFBO);
    }

    if(activeImage->imageType != NORMAL_TEXTURE){
        applyHeightProcessingFilter(activeFBO,activeAuxFBO);
        copyFBO(activeAuxFBO,activeFBO);
    }

    // -------------------------------------------------------- //
    // roughness color mapping
    // -------------------------------------------------------- //
    if(activeImage->imageType == ROUGHNESS_TEXTURE ||
       activeImage->imageType == METALLIC_TEXTURE){
        if(activeImage->bRoughnessEnableColorPicking && !activeImage->bRoughnessColorPickingToggled){
            applyRoughnessColorFilter(activeFBO,activeAuxFBO);
            copyFBO(activeAuxFBO,activeFBO);
        }
    }
    // -------------------------------------------------------- //
    // height processing pipeline
    // -------------------------------------------------------- //
    if(activeImage->imageType == HEIGHT_TEXTURE){
        // processing

        //applyCPUNormalizationFilter(activeAuxFBO,activeFBO);
        // conversion step
        if(activeImage->bConversionHN){
        //applyHeightToNormal(activeFBO,activeAuxFBO);
        //copyFBO(activeAuxFBO,targetImageNormal->fbo);
        }
    }
    // -------------------------------------------------------- //
    // normal processing pipeline
    // -------------------------------------------------------- //
    if(activeImage->imageType == NORMAL_TEXTURE){

        applyNormalsStepFilter(activeFBO,activeAuxFBO);
        copyFBO(activeAuxFBO,activeFBO);

        /*
        // conversion
        if(activeImage->bConversionNH){
            applyNormalToHeight(activeImage,activeFBO,activeAux2FBO,activeAuxFBO);
            applyCPUNormalizationFilter(activeAuxFBO,activeFBO);
        }
        */
    }
    // -------------------------------------------------------- //
    // diffuse processing pipeline
    // -------------------------------------------------------- //
    if(activeImage->imageType == DIFFUSE_TEXTURE && activeImage->bConversionBaseMap){
        applyBaseMapConversion(activeFBO,activeAux2FBO,activeAuxFBO);

        if(conversionType == CONVERT_FROM_D_TO_O){
            applyNormalToHeight(targetImageHeight,activeAuxFBO,activeFBO,activeAux2FBO);
            applyCPUNormalizationFilter(activeAux2FBO,activeFBO);
        }

        copyFBO(activeFBO,activeAux2FBO);
        copyFBO(activeAuxFBO,activeFBO);


    }

    }// end of skip standard processing

    // copying the conversion results to proper textures
    switch(conversionType){
        case(CONVERT_FROM_H_TO_N):
        if(activeImage->imageType == NORMAL_TEXTURE){
            copyFBO(activeFBO,targetImageNormal->ref_fbo);
            copyFBO(activeFBO,targetImageNormal->fbo);

            program->setUniformValue("gui_clear_alpha",1);
            applyNormalFilter(targetImageNormal->ref_fbo,activeFBO);
            program->setUniformValue("gui_clear_alpha",0);
            targetImageNormal->updateSrcTexId(activeFBO);
        }

        break;
        case(CONVERT_FROM_N_TO_H):
            if(activeImage->imageType == HEIGHT_TEXTURE){
                qDebug() << "Changing reference image of height";
                //copyFBO(activeFBO,targetImageHeight->ref_fbo);
                targetImageHeight->updateSrcTexId(targetImageHeight->ref_fbo);
            }
        break;
        case(CONVERT_FROM_D_TO_O):        
        copyFBO(activeFBO,targetImageNormal->fbo);
        copyFBO(activeFBO,targetImageNormal->ref_fbo);

        program->setUniformValue("gui_clear_alpha",1);
        applyNormalFilter(targetImageNormal->ref_fbo,activeFBO);
        program->setUniformValue("gui_clear_alpha",0);
        targetImageNormal->updateSrcTexId(activeFBO);

        copyFBO(activeAux2FBO,targetImageHeight->ref_fbo);
        copyFBO(activeAux2FBO,targetImageHeight->fbo);
        targetImageHeight->updateSrcTexId(activeAux2FBO);


        applyCombineNormalHeightFilter(targetImageNormal->ref_fbo,targetImageHeight->ref_fbo,activeAuxFBO);
        applyOcclusionFilter(activeAuxFBO,targetImageOcclusion->ref_fbo);
        copyFBO(targetImageOcclusion->ref_fbo,targetImageOcclusion->fbo);
        //copyFBO(targetImageHeight->fbo,targetImageOcclusion->ref_fbo);
        targetImageOcclusion->updateSrcTexId(targetImageOcclusion->ref_fbo);

        copyFBO(activeImage->ref_fbo,targetImageSpecular->ref_fbo);
        copyFBO(activeImage->ref_fbo,targetImageSpecular->fbo);
        targetImageSpecular->updateSrcTexId(activeImage->ref_fbo);

        copyFBO(activeImage->ref_fbo,targetImageRoughness->ref_fbo);
        copyFBO(activeImage->ref_fbo,targetImageRoughness->fbo);
        targetImageRoughness->updateSrcTexId(activeImage->ref_fbo);

        copyFBO(activeImage->ref_fbo,targetImageMetallic->ref_fbo);
        copyFBO(activeImage->ref_fbo,targetImageMetallic->fbo);
        targetImageMetallic->updateSrcTexId(activeImage->ref_fbo);

        break;
        case(CONVERT_FROM_HN_TO_OC):
        copyFBO(activeFBO,targetImageOcclusion->ref_fbo);
        copyFBO(activeFBO,targetImageOcclusion->fbo);

        program->setUniformValue("gui_clear_alpha",1);
        applyNormalFilter(targetImageOcclusion->ref_fbo,activeFBO);
        program->setUniformValue("gui_clear_alpha",0);
        targetImageOcclusion->updateSrcTexId(activeFBO);

        break;

        default:
        break;
    }


    activeFBO = activeImage->fbo;
    GLCHK( program->setUniformValue("gui_clear_alpha",1) );
    applyNormalFilter(activeFBO,activeAuxFBO);
    GLCHK( program->setUniformValue("gui_clear_alpha",0) );
    copyFBO(activeAuxFBO,activeFBO);

    //qDebug() << PostfixNames::getTextureName(activeImage->imageType) << " output";

    }// end of skip processing

    /*
    // In case of preview mode show the other texture
    if(activeImage->bConversionHN){
        activeFBO = targetImageNormal->fbo;
    }
    */

    if(!bShadowRender){
        // Displaying new image
        activeFBO->bindDefault();
        program->setUniformValue("quad_draw_mode", 1);

        GLCHK( glViewport(0,0,width(),height()) );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, activeFBO->texture()) );

        QMatrix4x4 m;        
        m.ortho(0,orthographicProjWidth,0,orthographicProjHeight,-1,1);
        GLCHK( program->setUniformValue("ProjectionMatrix", m) );
        m.setToIdentity();
        m.translate(xTranslation,yTranslation,0);
        GLCHK( program->setUniformValue("ModelViewMatrix", m) );
        GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_normal_filter"]) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
        GLCHK( program->setUniformValue("quad_draw_mode", int(0)) );

        
    }
    bSkipProcessing = false;
    conversionType  = CONVERT_NONE;

}

void GLImage::showEvent(QShowEvent* event){
    QWidget::showEvent( event );
    resetView();
}
void GLImage::resizeFBO(int width, int height){

     conversionType = CONVERT_RESIZE;
     resize_width   = width;
     resize_height  = height;
     updateGL();
     conversionType = CONVERT_NONE;
}

void GLImage::resetView(){

    if (!activeImage) return;

    makeCurrent();

    zoom = 0;
    windowRatio = float(width())/height();
    fboRatio    = float(activeImage->fbo->width())/activeImage->fbo->height();
    // openGL window dimensions
    orthographicProjHeight = (1+zoom)/windowRatio;
    orthographicProjWidth = (1+zoom)/fboRatio;

    if(orthographicProjWidth < 1.0) { // fitting x direction
        zoom = fboRatio - 1;
        orthographicProjHeight = (1+zoom)/windowRatio;
        orthographicProjWidth = (1+zoom)/fboRatio;
    }
    if(orthographicProjHeight < 1.0) { // fitting y direction
        zoom = windowRatio - 1;
        orthographicProjHeight = (1+zoom)/windowRatio;
        orthographicProjWidth = (1+zoom)/fboRatio;
    }
    // setting the image in the center
    xTranslation = orthographicProjWidth /2;
    yTranslation = orthographicProjHeight/2;
}

void GLImage::resizeGL(int width, int height)
{
  windowRatio = float(width)/height;
  if (isValid()) {
    GLCHK( glViewport(0, 0, width, height) );

    if (activeImage && activeImage->fbo){
      fboRatio = float(activeImage->fbo->width())/activeImage->fbo->height();
      orthographicProjHeight = (1+zoom)/windowRatio;
      orthographicProjWidth = (1+zoom)/fboRatio;
    } else {
      qWarning() << Q_FUNC_INFO;
      if (!activeImage) qWarning() << "  activeImage is null";
      else
	if (!activeImage->fbo) qWarning() << "  activeImage->fbo is null";
    }
  } else
    qDebug() << Q_FUNC_INFO << "invalid context.";

  resetView();
}


void GLImage::setActiveImage(FBOImageProporties* ptr){
        activeImage = ptr;
        updateGL();
}
void GLImage::enableShadowRender(bool enable){
        bShadowRender = enable;
}
void GLImage::setConversionType(ConversionType type){
    conversionType = type ;
}
void GLImage::updateCornersPosition(QVector2D dc1,QVector2D dc2,QVector2D dc3,QVector2D dc4){

    cornerPositions[0] = QVector2D(0,0) + dc1;
    cornerPositions[1] = QVector2D(1,0) + dc2;
    cornerPositions[2] = QVector2D(1,1) + dc3;
    cornerPositions[3] = QVector2D(0,1) + dc4;
    updateGL();
}
void GLImage::selectPerspectiveTransformMethod(int method){
    gui_perspective_mode = method;
    updateGL();
}

void GLImage::selectUVManipulationMethod(UVManipulationMethods method){
    uvManilupationMethod = method;
    updateGL();
}

void GLImage::updateCornersWeights(float w1,float w2,float w3,float w4){
    cornerWeights.setX( w1);
    cornerWeights.setY( w2);
    cornerWeights.setZ( w3);
    cornerWeights.setW( w4);
    updateGL();
}

void GLImage::selectSeamlessMode(SeamlessMode mode){
    FBOImageProporties::seamlessMode = mode;
    updateGL();
}


void GLImage::applyNormalFilter(QGLFramebufferObject* inputFBO,
                         QGLFramebufferObject* outputFBO){

    GLCHK( outputFBO->bind() );
    GLCHK( glViewport(0,0,outputFBO->width(),outputFBO->height()) );
    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_normal_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    outputFBO->bindDefault();
}

void GLImage::applyColorHueFilter(  QGLFramebufferObject* inputFBO,
                           QGLFramebufferObject* outputFBO){

    GLCHK( outputFBO->bind() );
    GLCHK( glViewport(0,0,outputFBO->width(),outputFBO->height()) );
    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_color_hue_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( program->setUniformValue("gui_hue"   , float(activeImage->colorHue)) );

    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    outputFBO->bindDefault();
}

void GLImage::applyPerspectiveTransformFilter(  QGLFramebufferObject* inputFBO,
                                                QGLFramebufferObject* outputFBO){

    GLCHK( outputFBO->bind() );

    GLCHK( glViewport(0,0,outputFBO->width(),outputFBO->height()) );
    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_perspective_transform_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( program->setUniformValue("corner1"  , cornerPositions[0]) );
    GLCHK( program->setUniformValue("corner2"  , cornerPositions[1]) );
    GLCHK( program->setUniformValue("corner3"  , cornerPositions[2]) );
    GLCHK( program->setUniformValue("corner4"  , cornerPositions[3]) );
    GLCHK( program->setUniformValue("corners_weights"  , cornerWeights) );
    GLCHK( program->setUniformValue("uv_scaling_mode", 0) );
    GLCHK( program->setUniformValue("gui_perspective_mode"  , gui_perspective_mode) );


    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );

    GLCHK( inputFBO->bind() );
    GLCHK( glViewport(0,0,outputFBO->width(),outputFBO->height()) );
    GLCHK( program->setUniformValue("uv_scaling_mode", 1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, outputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    inputFBO->bindDefault();


}



void GLImage::applyCompressedFormatFilter(QGLFramebufferObject* baseFBO,
                                          QGLFramebufferObject* alphaFBO,
                                          QGLFramebufferObject* outputFBO){




    GLCHK( outputFBO->bind() );
    GLCHK( glViewport(0,0,outputFBO->width(),outputFBO->height()) );
    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_compressed_type_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, baseFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, alphaFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    outputFBO->bindDefault();
}

void GLImage::applyGaussFilter(QGLFramebufferObject* sourceFBO,
                               QGLFramebufferObject* auxFBO,
                               QGLFramebufferObject* outputFBO,int no_iter,float w ){


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_gauss_filter"]) );
    GLCHK( program->setUniformValue("gui_gauss_radius", no_iter) );
    if( w == 0){
        GLCHK( program->setUniformValue("gui_gauss_w", float(no_iter)) );
    }else
        GLCHK( program->setUniformValue("gui_gauss_w", float(w)) );

    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( glViewport(0,0,sourceFBO->width(),sourceFBO->height()) );
    GLCHK( program->setUniformValue("gauss_mode",1) );

    GLCHK( auxFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, sourceFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( program->setUniformValue("gauss_mode",2) );
    GLCHK( outputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, auxFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( program->setUniformValue("gauss_mode",0) );


}

void GLImage::applyMaskedGaussFilter(
                            QGLFramebufferObject* sourceFBO,
                            QGLFramebufferObject* maskFBO,
                            QGLFramebufferObject *auxFBO,
                            QGLFramebufferObject *aux2FBO,
                            QGLFramebufferObject* outputFBO){


    // blur orginal image
    applyGaussFilter(sourceFBO,outputFBO,auxFBO,int(activeImage->selectiveBlurMaskRadius));

    for(int iters = 0 ; iters < activeImage->selectiveBlurNoIters - 1 ; iters ++ ){
        applyGaussFilter(auxFBO,aux2FBO,outputFBO,int(activeImage->selectiveBlurMaskRadius));
        applyGaussFilter(outputFBO,aux2FBO,auxFBO,int(activeImage->selectiveBlurMaskRadius));
    }

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_gauss_filter"]) );

    GLCHK( program->setUniformValue("gui_gauss_radius"   ,int  (activeImage->selectiveBlurMaskRadius)) );
    GLCHK( program->setUniformValue("gui_gauss_w"        ,float(activeImage->selectiveBlurMaskRadius)) );
    GLCHK( program->setUniformValue("gui_gauss_mask"  , 1) );
    GLCHK( program->setUniformValue("gui_gauss_show_mask",bool (activeImage->bSelectiveBlurPreviewMask) ) );
    GLCHK( program->setUniformValue("gui_gauss_blending",(activeImage->selectiveBlurBlending) ) );
    GLCHK( program->setUniformValue("gui_gauss_invert_mask",bool (activeImage->bSelectiveBlurInvertMask) ) );



    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( glViewport(0,0,sourceFBO->width(),sourceFBO->height()) );

    GLCHK( outputFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, sourceFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, maskFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE2) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, auxFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );

    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( program->setUniformValue("gauss_mode",0) );
    GLCHK( program->setUniformValue("gui_gauss_mask"  , 0) );
    GLCHK( program->setUniformValue("gui_gauss_show_mask" , false ) );
    GLCHK( outputFBO->bindDefault() );
}


void GLImage::applyInverseColorFilter(QGLFramebufferObject* inputFBO,
                                      QGLFramebufferObject* outputFBO){


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_invert_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( outputFBO->bindDefault() );
}

void GLImage::applyRemoveShadingFilter(QGLFramebufferObject* inputFBO,
                               QGLFramebufferObject* aoMaskFBO,
                               QGLFramebufferObject* refFBO,
                               QGLFramebufferObject* outputFBO){

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_ao_cancellation_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( program->setUniformValue("gui_remove_shading",activeImage->noRemoveShadingGaussIter));
    GLCHK( program->setUniformValue("gui_ao_cancellation",activeImage->aoCancellation ));

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, aoMaskFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE2) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, refFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( outputFBO->bindDefault() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
}

void GLImage::applyOverlayFilter(QGLFramebufferObject* layerAFBO,
                                 QGLFramebufferObject* layerBFBO,
                                 QGLFramebufferObject* outputFBO){


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_overlay_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( outputFBO->bind() );
    GLCHK( glViewport(0,0,outputFBO->width(),outputFBO->height()) );

    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, layerAFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, layerBFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );

    outputFBO->bindDefault();

}

void GLImage::applySeamlessFilter(QGLFramebufferObject* inputFBO,
                                  QGLFramebufferObject* outputFBO){

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_seamless_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( program->setUniformValue("make_seamless_radius"      , FBOImageProporties::seamlessSimpleModeRadius) );
    GLCHK( program->setUniformValue("gui_seamless_mode"         , (int)FBOImageProporties::seamlessMode) );
    GLCHK( program->setUniformValue("gui_seamless_mirror_type"  , FBOImageProporties::seamlessMirroModeType) );

    // sending the random angles
    QMatrix3x3 random_angles;
    for(int i = 0; i < 9; i++)random_angles.data()[i] = FBOImageProporties::seamlessRandomTiling.angles[i];
    GLCHK( program->setUniformValue("gui_seamless_random_angles" , random_angles) );
    GLCHK( program->setUniformValue("gui_seamless_random_phase" , FBOImageProporties::seamlessRandomTiling.common_phase) );
    GLCHK( program->setUniformValue("gui_seamless_random_inner_radius" , FBOImageProporties::seamlessRandomTiling.inner_radius) );
    GLCHK( program->setUniformValue("gui_seamless_random_outer_radius" , FBOImageProporties::seamlessRandomTiling.outer_radius) );

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );

    outputFBO->bindDefault();
}

void GLImage::applyDGaussiansFilter(QGLFramebufferObject* inputFBO,
                                  QGLFramebufferObject* auxFBO,
                                  QGLFramebufferObject* outputFBO,bool bUseSelectiveBlur){


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_gauss_filter"]) );
    if(bUseSelectiveBlur){
    GLCHK( program->setUniformValue("gui_gauss_radius", int(activeImage->selectiveBlurDOGRadius)) );
    GLCHK( program->setUniformValue("gui_gauss_w", float(0.1)) );
    }else{
    GLCHK( program->setUniformValue("gui_gauss_radius", int(activeImage->specularRadius)) );
    GLCHK( program->setUniformValue("gui_gauss_w", activeImage->specularW1) );
    }
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( program->setUniformValue("gauss_mode",1) );

    GLCHK( auxFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( program->setUniformValue("gauss_mode",2) );
    GLCHK( outputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, auxFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );

    if(bUseSelectiveBlur){
    GLCHK( program->setUniformValue("gui_gauss_w", float(activeImage->selectiveBlurDOGRadius)) );
    }else{
    GLCHK( program->setUniformValue("gui_gauss_w", activeImage->specularW2) );
    }
    GLCHK( auxFBO->bind() );
    GLCHK( program->setUniformValue("gauss_mode",1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( program->setUniformValue("gauss_mode",2) );
    GLCHK( inputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, auxFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_dgaussians_filter"]) );
    if(bUseSelectiveBlur){
    GLCHK( program->setUniformValue("gui_specular_amplifier", activeImage->selectiveBlurDOGAmplifier) );
    }else{
    GLCHK( program->setUniformValue("gui_specular_amplifier", activeImage->specularAmplifier) );
    }


    GLCHK( auxFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, outputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( program->setUniformValue("gauss_mode",0) );

    copyFBO(auxFBO,outputFBO);
    outputFBO->bindDefault();

}

void GLImage::applyContrastFilter(QGLFramebufferObject* inputFBO,
                                  QGLFramebufferObject* outputFBO, bool bUseSelectiveBlur){

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_constrast_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
        if(bUseSelectiveBlur){
        GLCHK( program->setUniformValue("gui_specular_contrast"  , activeImage->selectiveBlurDOGConstrast) );
        GLCHK( program->setUniformValue("gui_specular_brightness", activeImage->selectiveBlurDOGOffset) );
    }else{
        GLCHK( program->setUniformValue("gui_specular_contrast", activeImage->specularContrast) );
        GLCHK( program->setUniformValue("gui_specular_brightness", activeImage->specularBrightness) );
    }
    
    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( outputFBO->bindDefault());
}

void GLImage::applySmallDetailsFilter(QGLFramebufferObject* inputFBO,
                                      QGLFramebufferObject* auxFBO,
                                    QGLFramebufferObject* outputFBO){


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_gauss_filter"]) );
    GLCHK( program->setUniformValue("gui_depth", activeImage->detailDepth) );
    GLCHK( program->setUniformValue("gui_gauss_radius", int(3.0)) );
    GLCHK( program->setUniformValue("gui_gauss_w", float(3.0)) );

    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( program->setUniformValue("gauss_mode",1) );

    GLCHK( auxFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( program->setUniformValue("gauss_mode",2) );
    GLCHK( outputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, auxFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_dgaussians_filter"]) );
    GLCHK( program->setUniformValue("gui_mode_dgaussian", 0) );

    GLCHK( auxFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, outputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( program->setUniformValue("gauss_mode",0) );
    GLCHK( program->setUniformValue("gui_mode_dgaussian", 1) );




    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_small_details_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( program->setUniformValue("gui_small_details", activeImage->smallDetails) );
    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, auxFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( outputFBO->bindDefault() );
    GLCHK( program->setUniformValue("gui_depth", float(1.0)) );

}

void GLImage::applyMediumDetailsFilter(QGLFramebufferObject* inputFBO,
                                       QGLFramebufferObject* auxFBO,
                                       QGLFramebufferObject* outputFBO){


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_gauss_filter"]) );
    GLCHK( program->setUniformValue("gui_depth", activeImage->detailDepth) );
    GLCHK( program->setUniformValue("gui_gauss_radius", int(15.0)) );
    GLCHK( program->setUniformValue("gui_gauss_w", float(15.0)) );

    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( program->setUniformValue("gauss_mode",1) );

    GLCHK( auxFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( program->setUniformValue("gauss_mode",2) );
    GLCHK( outputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, auxFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );


    GLCHK( program->setUniformValue("gauss_mode",1) );
    GLCHK( program->setUniformValue("gui_gauss_radius", int(20.0)) );
    GLCHK( program->setUniformValue("gui_gauss_w"     , float(20.0)) );

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_medium_details_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    program->setUniformValue("gui_small_details", activeImage->mediumDetails);
    glViewport(0,0,inputFBO->width(),inputFBO->height());
    GLCHK( auxFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, outputFBO->texture()) );

    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( outputFBO->bindDefault() );
    GLCHK( program->setUniformValue("gui_depth", float(1.0)) );
    copyFBO(auxFBO,outputFBO);


}


void GLImage::applyGrayScaleFilter(QGLFramebufferObject* inputFBO,
                             QGLFramebufferObject* outputFBO){

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_gray_scale_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( program->setUniformValue("gui_gray_scale_preset",activeImage->grayScalePreset.toQVector3D()) );
    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( outputFBO->bindDefault() );

}

void GLImage::applyInvertComponentsFilter(QGLFramebufferObject* inputFBO,
                             QGLFramebufferObject* outputFBO){

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_invert_components_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( program->setUniformValue("gui_inverted_components"  , QVector3D(activeImage->bInvertR,activeImage->bInvertG,activeImage->bInvertB)) );

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( outputFBO->bindDefault() );

}

void GLImage::applySharpenBlurFilter(QGLFramebufferObject* inputFBO,
                                     QGLFramebufferObject* auxFBO,
                                     QGLFramebufferObject* outputFBO){

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_sharpen_blur"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( program->setUniformValue("gui_sharpen_blur", activeImage->sharpenBlurAmount) );

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );

    GLCHK( auxFBO->bind() );
    GLCHK( program->setUniformValue("gauss_mode",1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( outputFBO->bind() );
    GLCHK( program->setUniformValue("gauss_mode",2) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, auxFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );

    GLCHK( outputFBO->bindDefault() );
    GLCHK( program->setUniformValue("gauss_mode",0) );

}

void GLImage::applyNormalsStepFilter(QGLFramebufferObject* inputFBO,
                               QGLFramebufferObject* outputFBO){

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_normals_step_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( program->setUniformValue("gui_normals_step", activeImage->normalsStep) );

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( outputFBO->bindDefault() );

}


void GLImage::applyPreSmoothFilter(  QGLFramebufferObject* inputFBO,
                                     QGLFramebufferObject* auxFBO,
                                     QGLFramebufferObject* outputFBO){


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_gauss_filter"]) );
    GLCHK( program->setUniformValue("gui_gauss_radius", int(activeImage->conversionBaseMapPreSmoothRadius)) );
    GLCHK( program->setUniformValue("gui_gauss_w", float(activeImage->conversionBaseMapPreSmoothRadius)) );


    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( program->setUniformValue("gauss_mode",1) );

    GLCHK( auxFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( program->setUniformValue("gauss_mode",2) );
    GLCHK( outputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, auxFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( program->setUniformValue("gauss_mode",0) );
    GLCHK( outputFBO->bindDefault() );


}

void GLImage::applySobelToNormalFilter(QGLFramebufferObject* inputFBO,
                                       QGLFramebufferObject* outputFBO){


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_sobel_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( program->setUniformValue("gui_basemap_amp", activeImage->conversionBaseMapAmplitude) );

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( outputFBO->bindDefault() );

}

void GLImage::applyHeightToNormal(QGLFramebufferObject* inputFBO,
                         QGLFramebufferObject* outputFBO){

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_height_to_normal"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( program->setUniformValue("gui_hn_conversion_depth", activeImage->conversionHNDepth) );
    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( outputFBO->bindDefault() );

}


void GLImage::applyNormalToHeight(FBOImageProporties* image,QGLFramebufferObject* normalFBO,
                                  QGLFramebufferObject* heightFBO,
                                  QGLFramebufferObject* outputFBO){



    applyGrayScaleFilter(normalFBO,heightFBO);
    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_normal_to_height"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );



    GLCHK( outputFBO->bind() );
    GLCHK( glViewport(0,0,heightFBO->width(),heightFBO->height()) );
    GLCHK( program->setUniformValue("hn_min_max_scale",QVector3D(-0.0,1.0,1.0)) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, heightFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, normalFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );

    for(int i = 0; i < image->conversionNHItersHuge ; i++){
        GLCHK( program->setUniformValue("hn_min_max_scale",QVector3D(-0.0,1.0,pow(2.0,5))) );
        GLCHK( heightFBO->bind() );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, outputFBO->texture()) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );

        GLCHK( outputFBO->bind() );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, heightFBO->texture()) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    }
    for(int i = 0; i < image->conversionNHItersVeryLarge ; i++){
        GLCHK( program->setUniformValue("hn_min_max_scale",QVector3D(-0.0,1.0,pow(2.0,4))) );
        GLCHK( heightFBO->bind() );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, outputFBO->texture()) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );

        GLCHK( outputFBO->bind() );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, heightFBO->texture()) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    }
    for(int i = 0; i < image->conversionNHItersLarge ; i++){
        GLCHK( program->setUniformValue("hn_min_max_scale",QVector3D(-0.0,1.0,pow(2.0,3))) );
        GLCHK( heightFBO->bind() );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, outputFBO->texture()) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );

        GLCHK( outputFBO->bind() );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, heightFBO->texture()) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    }
    for(int i = 0; i < image->conversionNHItersMedium; i++){
        GLCHK( program->setUniformValue("hn_min_max_scale",QVector3D(-0.0,1.0,pow(2.0,2))) );
        GLCHK( heightFBO->bind() );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, outputFBO->texture()) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );

        GLCHK( outputFBO->bind() );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, heightFBO->texture()) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    }
    for(int i = 0; i < image->conversionNHItersSmall; i++){
        GLCHK( program->setUniformValue("hn_min_max_scale",QVector3D(-0.0,1.0,pow(2.0,1))) );
        GLCHK( heightFBO->bind() );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, outputFBO->texture()) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );

        GLCHK( outputFBO->bind() );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, heightFBO->texture()) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    }
    for(int i = 0; i < image->conversionNHItersVerySmall; i++){
        GLCHK( program->setUniformValue("hn_min_max_scale",QVector3D(-0.0,1.0,pow(2.0,0))) );
        GLCHK( heightFBO->bind() );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, outputFBO->texture()) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );

        GLCHK( outputFBO->bind() );
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, heightFBO->texture()) );
        GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    }

    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( outputFBO->bindDefault() );
}

void GLImage::applySelectiveGaussFilter(QGLFramebufferObject* inputFBO,
                              QGLFramebufferObject* outputFBO){


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_normal_expansion_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( outputFBO->bindDefault() );

}

void GLImage::applyCPUNormalizationFilter(QGLFramebufferObject* inputFBO,
                                          QGLFramebufferObject* outputFBO){

    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLint textureWidth, textureHeight;
    GLCHK( glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &textureWidth) );
    GLCHK( glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &textureHeight) );

    float* img = new float[textureWidth*textureHeight*4];

    GLCHK( glGetTexImage(	GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,img) );

    float min[3] = {img[0],img[1],img[2]};
    float max[3] = {img[0],img[1],img[2]};

    for(int i = 0 ; i < textureWidth*textureHeight ; i++){
        for(int c = 0 ; c < 3 ; c++){
            if( max[c] < img[4*i+c] ) max[c] = img[4*i+c];
            if( min[c] > img[4*i+c] ) min[c] = img[4*i+c];
        }

    }

    qDebug() << "Image normalization:";
    qDebug() << "Min color = (" << min[0] << "," << min[1] << "," << min[2] << ")"  ;
    qDebug() << "Max color = (" << max[0] << "," << max[1] << "," << max[2] << ")"  ;


    delete[] img;


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_normalize_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( outputFBO->bind() );
    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( program->setUniformValue("min_color",QVector3D(min[0],min[1],min[2])) );
    GLCHK( program->setUniformValue("max_color",QVector3D(max[0],max[1],max[2])) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );


    GLCHK( outputFBO->bindDefault() );

}

void GLImage::applyBaseMapConversion(QGLFramebufferObject* baseMapFBO,
                                     QGLFramebufferObject *auxFBO,
                                     QGLFramebufferObject* outputFBO){


        applyGrayScaleFilter(baseMapFBO,outputFBO);
        applySobelToNormalFilter(outputFBO,auxFBO);
        applyInvertComponentsFilter(auxFBO,baseMapFBO);
        applyPreSmoothFilter(baseMapFBO,auxFBO,outputFBO);

        GLCHK( program->setUniformValue("gui_combine_normals" , 0) );
        GLCHK( program->setUniformValue("gui_filter_radius" , activeImage->conversionBaseMapFilterRadius) );

        for(int i = 0; i < activeImage->conversionBaseMapNoIters ; i ++){
            copyFBO(outputFBO,auxFBO);
            GLCHK( program->setUniformValue("gui_normal_flatting" , activeImage->conversionBaseMapFlatness) );
            applySelectiveGaussFilter(auxFBO,outputFBO);
        }

        GLCHK( program->setUniformValue("gui_combine_normals" , 1) );
        GLCHK( program->setUniformValue("gui_mix_normals"   , activeImage->conversionBaseMapMixNormals) );
        GLCHK( program->setUniformValue("gui_blend_normals" , activeImage->conversionBaseMapBlending) );
        copyFBO(outputFBO,auxFBO);
        GLCHK( glActiveTexture(GL_TEXTURE1) );
        GLCHK( glBindTexture(GL_TEXTURE_2D, baseMapFBO->texture()) );
        applySelectiveGaussFilter(auxFBO,outputFBO);
        GLCHK( glActiveTexture(GL_TEXTURE0) );
        GLCHK( program->setUniformValue("gui_combine_normals" , 0 ) );


}


void GLImage::applyOcclusionFilter(QGLFramebufferObject* inputFBO,
                          QGLFramebufferObject* outputFBO){

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_occlusion_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );

    GLCHK( program->setUniformValue("gui_ssao_no_iters"   ,targetImageOcclusion->ssaoNoIters) );
    GLCHK( program->setUniformValue("gui_ssao_depth"      ,targetImageOcclusion->ssaoDepth) );
    GLCHK( program->setUniformValue("gui_ssao_bias"       ,targetImageOcclusion->ssaoBias) );
    GLCHK( program->setUniformValue("gui_ssao_intensity"  ,targetImageOcclusion->ssaoIntensity) );

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, targetImageNormal->fbo->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE2) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, targetImageHeight->fbo->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( outputFBO->bindDefault() );

}
void GLImage::applyHeightProcessingFilter(QGLFramebufferObject* inputFBO,
                                           QGLFramebufferObject* outputFBO, bool bUseSelectiveBlur){

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_height_processing_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    if(bUseSelectiveBlur){
        GLCHK( program->setUniformValue("gui_height_proc_min_value"   ,activeImage->selectiveBlurMinValue) );
        GLCHK( program->setUniformValue("gui_height_proc_max_value"   ,activeImage->selectiveBlurMaxValue) );
        GLCHK( program->setUniformValue("gui_height_proc_ave_radius"  ,activeImage->selectiveBlurDetails) );
        GLCHK( program->setUniformValue("gui_height_proc_offset_value",activeImage->selectiveBlurOffsetValue) );
    }else{
        GLCHK( program->setUniformValue("gui_height_proc_min_value"   ,activeImage->heightMinValue) );
        GLCHK( program->setUniformValue("gui_height_proc_max_value"   ,activeImage->heightMaxValue) );
        GLCHK( program->setUniformValue("gui_height_proc_ave_radius"  ,activeImage->heightAveragingRadius) );
        GLCHK( program->setUniformValue("gui_height_proc_offset_value",activeImage->heightOffsetValue) );
    }
    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( outputFBO->bindDefault());
}

void GLImage::applyCombineNormalHeightFilter(QGLFramebufferObject* normalFBO,
                                             QGLFramebufferObject* heightFBO,
                                             QGLFramebufferObject* outputFBO){

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_combine_normal_height_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( glViewport(0,0,normalFBO->width(),normalFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, normalFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, heightFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    outputFBO->bindDefault();
}

void GLImage::applyRoughnessFilter(QGLFramebufferObject* inputFBO,
                                   QGLFramebufferObject* auxFBO,
                                    QGLFramebufferObject* outputFBO){

    // do the gaussian filter
    applyGaussFilter(inputFBO,auxFBO,outputFBO,int(activeImage->roughnessDepth));

    copyFBO(outputFBO,auxFBO);

    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_roughness_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( program->setUniformValue("gui_roughness_depth"     ,  activeImage->roughnessDepth ) );
    GLCHK( program->setUniformValue("gui_roughness_treshold"  ,  activeImage->roughnessTreshold) );
    GLCHK( program->setUniformValue("gui_roughness_amplifier"  ,  activeImage->roughnessAmplifier) );


    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glActiveTexture(GL_TEXTURE1) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, auxFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( outputFBO->bindDefault() );

}

void GLImage::applyRoughnessColorFilter(QGLFramebufferObject* inputFBO,
                                        QGLFramebufferObject* outputFBO){


    GLCHK( glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &subroutines["mode_roughness_color_filter"]) );
    GLCHK( program->setUniformValue("quad_scale", QVector2D(1.0,1.0)) );
    GLCHK( program->setUniformValue("quad_pos"  , QVector2D(0.0,0.0)) );
    GLCHK( program->setUniformValue("gui_roughness_picked_color"  , activeImage->pickedColor ) );
    GLCHK( program->setUniformValue("gui_roughness_color_method"  , activeImage->colorPickerMethod) );
    GLCHK( program->setUniformValue("gui_roughness_color_offset"  , activeImage->roughnessColorOffset) );
    GLCHK( program->setUniformValue("gui_roughness_color_global_offset"  , activeImage->roughnessColorGlobalOffset) );

    GLCHK( program->setUniformValue("gui_roughness_invert_mask"   , activeImage->bRoughnessInvertColorMask) );
    GLCHK( program->setUniformValue("gui_roughness_color_amplifier", activeImage->roughnessColorAmplifier) );

    GLCHK( glViewport(0,0,inputFBO->width(),inputFBO->height()) );
    GLCHK( outputFBO->bind() );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    GLCHK( glBindTexture(GL_TEXTURE_2D, inputFBO->texture()) );
    GLCHK( glDrawElements(GL_TRIANGLES, 3*2, GL_UNSIGNED_INT, 0) );
    GLCHK( glActiveTexture(GL_TEXTURE0) );
    outputFBO->bindDefault();

}

void GLImage::copyFBO(QGLFramebufferObject* src,QGLFramebufferObject* dst){
    src->blitFramebuffer(dst,QRect(QPoint(0,0),src->size()),src,QRect(QPoint(0,0),dst->size()));
}

void GLImage::makeScreenQuad()
{

    int size = 2;
    QVector<QVector2D> texCoords = QVector<QVector2D>(size*size);
    QVector<QVector3D> vertices  = QVector<QVector3D>(size*size);
    int iter = 0;
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            float offset = 0.5;
            float x = i/(size-1.0);
            float y = j/(size-1.0);
            vertices[iter]  = (QVector3D(x-offset,y-offset,0));
            texCoords[iter] = (QVector2D(x,y));
            iter++;
    }}



    glGenBuffers(3, &vbos[0]);
    glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float)*3, vertices.constData(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(float)*3,(void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
    glBufferData(GL_ARRAY_BUFFER, texCoords.size() * sizeof(float)*2, texCoords.constData(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(float)*2,(void*)0);


    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos[2]);


    int no_triangles = 2*(size - 1)*(size - 1);
    QVector<GLuint> indices(no_triangles*3);
    iter = 0;
    for(int i = 0 ; i < size -1 ; i++){
    for(int j = 0 ; j < size -1 ; j++){
        GLuint i1 = i + j*size;
        GLuint i2 = i + (j+1)*size;
        GLuint i3 = i+1 + j*size;
        GLuint i4 = i+1 + (j+1)*size;
        indices[iter++] = (i1);
        indices[iter++] = (i3);
        indices[iter++] = (i2);
        indices[iter++] = (i2);
        indices[iter++] = (i3);
        indices[iter++] = (i4);
    }
    }
    GLCHK( glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * no_triangles * 3 , indices.constData(), GL_STATIC_DRAW) );
}


void GLImage::updateMousePosition(){
    QPoint p = mapFromGlobal(QCursor::pos());
    cursorPhysicalXPosition =       double(p.x())/width() * orthographicProjWidth  - xTranslation;
    cursorPhysicalYPosition = (1.0-double(p.y())/height())* orthographicProjHeight - yTranslation;
    bSkipProcessing = true;    
}

void GLImage::wheelEvent(QWheelEvent *event){

    if( event->delta() > 0) zoom-=0.1;
        else zoom+=0.1;
    if(zoom < -0.90) zoom = -0.90;

    updateMousePosition();

    //resizeGL(width(),height());
    windowRatio = float(width())/height();
    if (isValid()) {
      GLCHK( glViewport(0, 0, width(), height()) );

      if (activeImage && activeImage->fbo){
        fboRatio = float(activeImage->fbo->width())/activeImage->fbo->height();
        orthographicProjHeight = (1+zoom)/windowRatio;
        orthographicProjWidth = (1+zoom)/fboRatio;
      } else {
        qWarning() << Q_FUNC_INFO;
        if (!activeImage) qWarning() << "  activeImage is null";
        else
      if (!activeImage->fbo) qWarning() << "  activeImage->fbo is null";
      }
    } else
      qDebug() << Q_FUNC_INFO << "invalid context.";



    QPoint p = mapFromGlobal(QCursor::pos());//getting the global position of cursor
    // restoring the translation after zooming
    xTranslation =        double(p.x())/width() *orthographicProjWidth  - cursorPhysicalXPosition;
    yTranslation = ((1.0-double(p.y())/height())*orthographicProjHeight - cursorPhysicalYPosition );

    updateGL();
}

void GLImage::mouseMoveEvent(QMouseEvent *event)
{
    if(activeImage->imageType == OCCLUSION_TEXTURE && event->buttons() & Qt::LeftButton){
        QMessageBox msgBox;
        msgBox.setText("Warning!");
        msgBox.setInformativeText("Sorry, but you cannot modify UV's mapping of occlusion texture. Try Diffuse or height texture.");
        msgBox.setStandardButtons(QMessageBox::Cancel);
        msgBox.exec();
        return;
    }
    if(activeImage->imageType == NORMAL_TEXTURE && (event->buttons() & Qt::LeftButton)){
        QMessageBox msgBox;
        msgBox.setText("Warning!");
        msgBox.setInformativeText("Sorry, but you cannot modify UV's mapping of normal texture. Try Diffuse or height texture.");
        msgBox.setStandardButtons(QMessageBox::Cancel);
        msgBox.exec();
        return;
    }
    if(activeImage->imageType == METALLIC_TEXTURE && (event->buttons() & Qt::LeftButton)){
        QMessageBox msgBox;
        msgBox.setText("Warning!");
        msgBox.setInformativeText("Sorry, but you cannot modify UV's mapping of metallic texture. Try Diffuse or height texture.");
        msgBox.setStandardButtons(QMessageBox::Cancel);
        msgBox.exec();
        return;
    }

    if(activeImage->imageType == ROUGHNESS_TEXTURE && (event->buttons() & Qt::LeftButton)){
        QMessageBox msgBox;
        msgBox.setText("Warning!");
        msgBox.setInformativeText("Sorry, but you cannot modify UV's mapping of roughness texture. Try Diffuse or height texture.");
        msgBox.setStandardButtons(QMessageBox::Cancel);
        msgBox.exec();
        return;
    }
    if(activeImage->imageType == SPECULAR_TEXTURE && (event->buttons() & Qt::LeftButton)){
        QMessageBox msgBox;
        msgBox.setText("Warning!");
        msgBox.setInformativeText("Sorry, but you cannot modify UV's mapping of specular texture. Try Diffuse or height texture.");
        msgBox.setStandardButtons(QMessageBox::Cancel);
        msgBox.exec();
        return;
    }

    int dx = event->x() - lastCursorPos.x();
    int dy = event->y() - lastCursorPos.y();
    lastCursorPos = event->pos();

    QVector2D defCorners[4];//default position of corners
    defCorners[0] = QVector2D(0,0) ;
    defCorners[1] = QVector2D(1,0) ;
    defCorners[2] = QVector2D(1,1) ;
    defCorners[3] = QVector2D(0,1) ;
    QVector2D mpos((cursorPhysicalXPosition+0.5),(cursorPhysicalYPosition+0.5));

    // manipulate UV coordinates based on chosen method
    switch(uvManilupationMethod){
        // translate UV coordinates
        case(UV_TRANSLATE):
            if(event->buttons() & Qt::LeftButton){ // drag image
                setCursor(Qt::SizeAllCursor);
                // move all corners
                QVector2D averagePos(0.0,0.0);
                QVector2D dmouse = QVector2D(-dx*(float(orthographicProjWidth)/width()),dy*(float(orthographicProjHeight)/height()));
                for(int i = 0; i < 4 ; i++){
                    averagePos += cornerPositions[i]*0.25;
                    cornerPositions[i] += dmouse;
                }
                repaint();
            }
        break;
        // grab corners in perspective correction tool
        case(UV_GRAB_CORNERS):
            if(draggingCorner == -1){
            setCursor(Qt::OpenHandCursor);
            for(int i = 0; i < 4 ; i++){
                float dist = (mpos - defCorners[i]).length();
                if(dist < 0.2){
                    setCursor(cornerCursors[i]);
                }
            }
            }// end if dragging
            if(event->buttons() & Qt::LeftButton){
            // calculate distance from corners
            if(draggingCorner == -1){
            for(int i = 0; i < 4 ; i++){
                float dist = (mpos - defCorners[i]).length();
                if(dist < 0.2){
                    draggingCorner = i;
                }
            }// end of for corners
            }// end of if
            if(draggingCorner >=0 && draggingCorner < 4) cornerPositions[draggingCorner] += QVector2D(-dx*(float(orthographicProjWidth)/width()),dy*(float(orthographicProjHeight)/height()));
            repaint();
            }
        break;
        case(UV_SCALE_XY):
            setCursor(Qt::OpenHandCursor);
            if(event->buttons() & Qt::LeftButton){ // drag image
                setCursor(Qt::SizeAllCursor);
                QVector2D dmouse = QVector2D(-dx*(float(orthographicProjWidth)/width()),dy*(float(orthographicProjHeight)/height()));
                cornerWeights.setX(cornerWeights.x()-dmouse.x());
                cornerWeights.setY(cornerWeights.y()-dmouse.y());
                repaint();
            }
        break;
        default:;//no actions
    }




    if (event->buttons() & Qt::RightButton){
        xTranslation += dx*(float(orthographicProjWidth)/width());
        yTranslation -= dy*(float(orthographicProjHeight)/height());
        setCursor(Qt::ClosedHandCursor);
    }


    updateMousePosition();
    if(bToggleColorPicking){
        setCursor(Qt::UpArrowCursor);
    }

    updateGL();
}
void GLImage::mousePressEvent(QMouseEvent *event)
{

    lastCursorPos = event->pos();
    bSkipProcessing = true;
    draggingCorner = -1;
    // change cursor
    if (event->buttons() & Qt::RightButton) {
        setCursor(Qt::ClosedHandCursor);
    }
    updateGL();


    // In case of color picking: emit and stop picking
    if(bToggleColorPicking){
        vector< unsigned char > pixels( 1 * 1 * 4 );
        glReadPixels(event->pos().x(), height()-event->pos().y(), 1, 1,GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
        QVector4D color(pixels[0],pixels[1],pixels[2],pixels[3]);
        qDebug() << "Picked pixel (" << event->pos().x() << " , " << height()-event->pos().y() << ") with color:" << color;
        emit colorPicked(color);
        toggleColorPicking(false);
    }

}
void GLImage::mouseReleaseEvent(QMouseEvent *event){
    setCursor(Qt::OpenHandCursor);
    draggingCorner = -1;
    event->accept();
}

void GLImage::toggleColorPicking(bool toggle){
    bToggleColorPicking = toggle;
    if(toggle){
        setCursor(Qt::UpArrowCursor);
    }else
        setCursor(Qt::PointingHandCursor);
}
