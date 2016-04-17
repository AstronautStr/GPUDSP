#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "cinder/audio/audio.h"

#include "cinder/params/Params.h"
#include "Utils.h"

#define OPENCL 1

#define UNSAFEBUFFER 1
#define FIXEDBUFFER 0

#if OPENCL
#include "DSPOpenCL.h"
#else
#include "DSPOpenGL.h"
#endif

using namespace ci;
using namespace ci::app;
using namespace ci::audio;
using namespace std;

class ExternalDSPNode : public GenNode
{
protected:
    RingBuffer* _ringBuffer;
    DSPOpenCL* _controller;
    
public:
    ExternalDSPNode(RingBuffer* externalRingBuffer, DSPOpenCL* controller)
    {
        _ringBuffer = externalRingBuffer;
        _controller = controller;
    }
    
    void process(audio::Buffer* buffer)
    {
#if FIXEDBUFFER
        _controller->generateSamples(buffer->getData());
        return;
#else
        if (_ringBuffer)
        {
            float* data = buffer->getData();
#if LOGENABLED
            if (
#endif
            _ringBuffer->read(data, buffer->getNumFrames())
#if LOGENABLED
                == false)
                std::cerr << "[AudioThread]: BUFFERSKIP" << std::endl;
#else
            ;
#endif
            
        }
#endif
    }
};
typedef std::shared_ptr<class ExternalDSPNode> ExternalDSPNodeRef;


class AnotherSandboxProjectApp : public App
{
protected:
#if OPENCL
    DSPOpenCL* _DSPController;
#else
    DSPOpenGL* _DSPController;
#endif
    ExternalDSPNodeRef externalDSPNode;
    
    GLuint _drawingScreenSizeLoc;
    GLuint _drawingFragmentShader;
    GLuint _drawingVertexShader;
    GLuint _drawingProgram;
    
    GLuint _drawingVAO;
    GLuint _drawingVBO;
    GLuint _gridTex;
    GLuint _gridBuffer;
    
    GLfloat* _gridData;
    size_t _cellAttribCount;
    size_t _gridBufferLength;
    
    params::InterfaceGl _params;
    
    void _prepareDrawingProgram();
    void _prepareDrawingBuffers();
    void _prepareDrawingVertexArray();
    void _prepareDrawing();

    void _updateGridState();
    
    void _clearField();
    void _randomField();
    
  public:
    ~AnotherSandboxProjectApp();
    
	void setup() override;
    void keyDown( KeyEvent event ) override;
    
    void mouseMove( MouseEvent event ) override;
    void mouseDrag( MouseEvent event ) override;
    void mouseWheel( MouseEvent event ) override;
    void mouseUp( MouseEvent event ) override;
    void mouseDown( MouseEvent event ) override;
    
	void update() override;
	void draw() override;
    
    void modifyCell(vec2 cellPos, float value);
};


AnotherSandboxProjectApp::~AnotherSandboxProjectApp()
{
    delete _DSPController;
    delete [] _gridData;

    glDeleteProgram(_drawingProgram);
    glDeleteShader(_drawingFragmentShader);
    glDeleteShader(_drawingVertexShader);
    
    glDeleteBuffers(1, &_drawingVBO);
    glDeleteBuffers(1, &_gridBuffer);
    
    glDeleteTextures(1, &_gridTex);
    
    glDeleteVertexArrays(1, &_drawingVAO);
}


void AnotherSandboxProjectApp::_prepareDrawingProgram()
{
    std::string fragmentShaderSrcString = readAllText(PlatformCocoa::get()->getResourcePath("cellular.frag").string());
    GLchar const* const fragmentShaderSrc = fragmentShaderSrcString.c_str();
    _drawingFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(_drawingFragmentShader, 1, &fragmentShaderSrc, nullptr);
    glCompileShader(_drawingFragmentShader);
    std::cerr << lastError(_drawingFragmentShader) << std::endl;
    
    std::string vertexShaderSrcString = readAllText(PlatformCocoa::get()->getResourcePath("plain.vert").string());
    GLchar const* const vertexShaderSrc = vertexShaderSrcString.c_str();
    _drawingVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(_drawingVertexShader, 1, &vertexShaderSrc, nullptr);
    glCompileShader(_drawingVertexShader);
    std::cerr << lastError(_drawingVertexShader) << std::endl;
    
    _drawingProgram = glCreateProgram();
    glAttachShader(_drawingProgram, _drawingVertexShader);
    glAttachShader(_drawingProgram, _drawingFragmentShader);
    
    glLinkProgram(_drawingProgram);
    std::cerr << lastError(_drawingProgram) << std::endl;
    
    GLint gridSizeLoc = glGetUniformLocation(_drawingProgram, "gridSize");
    glProgramUniform2f(_drawingProgram, gridSizeLoc, (GLfloat)_DSPController->getGridSize().x, (GLfloat)_DSPController->getGridSize().y);
    
    _drawingScreenSizeLoc = glGetUniformLocation(_drawingProgram, "screenSize");
}
void AnotherSandboxProjectApp::_updateGridState()
{
    if (!_gridData)
    {
        _gridData = new GLfloat[_gridBufferLength];
        glGenBuffers(1, &_gridBuffer);
    }
    
    const DSPSampleType4* gridState = _DSPController->getCurrentGridState();
    for (size_t i = 0; i < _gridBufferLength; ++i)
    {
        _gridData[i] = (GLfloat)gridState[i / _cellAttribCount].s[i % _cellAttribCount];
    }

    glBindBuffer(GL_ARRAY_BUFFER, _gridBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * _gridBufferLength, _gridData, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
void AnotherSandboxProjectApp::_prepareDrawingBuffers()
{
    GLfloat plane[] =
    {  -1.0,   -1.0,   0.0,
        -1.0,   1.0,   0.0,
        1.0,   -1.0,   0.0,
        1.0,    1.0,   0.0
    };
    
    glGenBuffers(1, &_drawingVBO);
    glBindBuffer(GL_ARRAY_BUFFER, _drawingVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane), plane, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    _cellAttribCount = 4;
    _gridBufferLength = _cellAttribCount * _DSPController->getCellsCount();
    
    _gridData = NULL;
    _updateGridState();
    
    glGenTextures(1, &_gridTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, _gridTex);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, _gridBuffer);
}
void AnotherSandboxProjectApp::_prepareDrawingVertexArray()
{
    glGenVertexArrays(1, &_drawingVAO);
    glBindVertexArray(_drawingVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, _drawingVBO);
    
    GLint inPositionAttr = glGetAttribLocation(_drawingProgram, "ciPosition");
    glVertexAttribPointer(inPositionAttr, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(inPositionAttr);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void AnotherSandboxProjectApp::_prepareDrawing()
{
    _prepareDrawingProgram();
    _prepareDrawingBuffers();
    _prepareDrawingVertexArray();
}

void AnotherSandboxProjectApp::setup()
{
    srand(time(0));
    
    auto ctx = ci::audio::master();
    ci::audio::OutputNodeRef outputNode = ctx->getOutput();
    
    const size_t sampleRate = outputNode->getSampleRate();
    const size_t bufferSize = outputNode->getFramesPerBlock();
#if FIXEDBUFFER
    const size_t audioBuffersInGPUBuffer = 1;
    const size_t GPUBuffersInRingBuffer = 1;
#else
    const size_t audioBuffersInGPUBuffer = 4;
    const size_t GPUBuffersInRingBuffer = 3;
#endif
#if OPENCL
    _DSPController = new DSPOpenCL(sampleRate, bufferSize * audioBuffersInGPUBuffer * GPUBuffersInRingBuffer);
    externalDSPNode = ctx->makeNode(new ExternalDSPNode(&_DSPController->RingBuffer, _DSPController));
#else
    _DSPController = new DSPOpenGL(sampleRate, bufferSize * audioBuffersInGPUBuffer * GPUBuffersInRingBuffer);
    externalDSPNode = ctx->makeNode(new ExternalDSPNode(&_DSPController->RingBuffer));
#endif
    ci::audio::GainNodeRef gainNode = ctx->makeNode(new GainNode(1.0));
    
    externalDSPNode >> gainNode >> outputNode;
    
    externalDSPNode->enable();
    outputNode->enable();
    
    _prepareDrawing();
    
    _params = params::InterfaceGl("Parameters", ivec2(200, 400));
    _params.addParam("Rules: Birth center", _DSPController->getRulesBirthCenter(), "min=-10.0 max=10.0 step=0.001");
    _params.addParam("Rules: Birth radius", _DSPController->rulesBirthRadius(), "min=0.0 max=10.0 step=0.001");
    _params.addParam("Rules: Keep center", _DSPController->rulesKeepCenter(), "min=-10.0 max=10.0 step=0.001");
    _params.addParam("Rules: Keep radius", _DSPController->rulesKeepRadius(), "min=0.0 max=10.0 step=0.001");
    _params.addParam("Rules: Speed", _DSPController->rulesSpeed(), "min=0.0 max=1.0 step=0.001");
    
#if !FIXEDBUFFER
    _DSPController->generateSamples();
#endif
}


void AnotherSandboxProjectApp::_clearField()
{
    for (size_t i = 0; i < _DSPController->getCellsCount(); ++i)
    {
        _DSPController->DefferedUpdateGrid[i].s[0] = -1.0f;
        _DSPController->DefferedUpdateGrid[i].s[1] = 0.0f;
    }
}
void AnotherSandboxProjectApp::_randomField()
{
    for (size_t i = 0; i < _DSPController->getCellsCount(); ++i)
    {
        _DSPController->DefferedUpdateGrid[i].s[0] = randAmp();
        _DSPController->DefferedUpdateGrid[i].s[1] = randFreq();
    }
}

void AnotherSandboxProjectApp::modifyCell(vec2 screenPos, float value)
{
    ivec2 gridSize = _DSPController->getGridSize();
    ivec2 gridPoint = ivec2(math<int>::clamp(screenPos.x / getWindowWidth() * gridSize.x, 0, gridSize.x - 1), math<int>::clamp(screenPos.y / getWindowHeight() * gridSize.y, 0, gridSize.y - 1));
    size_t index = gridPoint.x * gridSize.y + gridPoint.y;
    
    _DSPController->DefferedUpdateGrid[index].s[0] = (DSPSampleType)value;
    _DSPController->DefferedUpdateGrid[index].s[1] = (DSPSampleType)randFreq();
}

void AnotherSandboxProjectApp::keyDown( KeyEvent event )
{
    switch (event.getCode())
    {
        case KeyEvent::KEY_SPACE:
            break;
            
        case KeyEvent::KEY_c:
            _clearField();
            break;
            
        case KeyEvent::KEY_r:
            _randomField();
            break;
            
        case KeyEvent::KEY_q:
            break;
            
        case KeyEvent::KEY_s:
            break;
            
        case KeyEvent::KEY_l:
            break;
            
        case KeyEvent::KEY_b:
            if (_params.isVisible())
                _params.hide();
            else
                _params.show();
            break;
    }
}

void AnotherSandboxProjectApp::mouseMove( MouseEvent event )
{
}

void AnotherSandboxProjectApp::mouseDrag( MouseEvent event )
{
    if (event.isLeft())
    {
        modifyCell(event.getPos(), 1.0f);
    }
    else if (event.isRight())
    {
        modifyCell(event.getPos(), -1.0f);
    }
}

void AnotherSandboxProjectApp::mouseWheel( MouseEvent event )
{
}

void AnotherSandboxProjectApp::mouseUp( MouseEvent event )
{
    if (event.isLeft())
    {
        modifyCell(event.getPos(), 1.0f);
    }
    else if (event.isRight())
    {
        modifyCell(event.getPos(), -1.0f);
    }
}

void AnotherSandboxProjectApp::mouseDown( MouseEvent event )
{
}

void AnotherSandboxProjectApp::update()
{
#if !FIXEDBUFFER
    _DSPController->generateSamples();
#endif
    
    _updateGridState();
}

void AnotherSandboxProjectApp::draw()
{
    gl::clear( Color( 0, 0, 0 ) );

    glUseProgram(_drawingProgram);
    glBindVertexArray(_drawingVAO);
    glBindBuffer(GL_ARRAY_BUFFER, _drawingVBO);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, _gridTex);
    glBindSampler(0, 0);
    
    glProgramUniform2f(_drawingProgram, _drawingScreenSizeLoc, (GLfloat)getWindowWidth(), (GLfloat)getWindowHeight());
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    vec2 cellRectSize = vec2((float)getWindowWidth() / (float)_DSPController->getGridSize().x, (float)getWindowHeight() / (float)_DSPController->getGridSize().x);
    ivec2 mouseCell = (ivec2)((vec2)getMousePos() / cellRectSize);
    vec2 mouseCellPoint = (vec2)mouseCell * cellRectSize;
    
    gl::color(1.0, 0.0, 0.0);
    gl::drawSolidRect(Rectf(mouseCellPoint, mouseCellPoint + cellRectSize));
    
    _params.draw();
}

CINDER_APP( AnotherSandboxProjectApp, RendererGl )
