#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/audio/audio.h"

#define UNSAFEBUFFER 1
#define OPENCL 1

#define FIXEDBUFFER 1

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
    
  public:
    ~AnotherSandboxProjectApp();
    
	void setup() override;
	void mouseDown( MouseEvent event ) override;
    void keyDown( KeyEvent event ) override;
	void update() override;
	void draw() override;
};


AnotherSandboxProjectApp::~AnotherSandboxProjectApp()
{
    delete _DSPController;
}

void AnotherSandboxProjectApp::setup()
{
    auto ctx = ci::audio::master();
    ci::audio::OutputNodeRef outputNode = ctx->getOutput();
    
    const size_t sampleRate = outputNode->getSampleRate();
    const size_t bufferSize = outputNode->getFramesPerBlock();
    
    const size_t audioBuffersInGPUBuffer = 1;
    const size_t GPUBuffersInRingBuffer = 1;
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
}

void AnotherSandboxProjectApp::keyDown( KeyEvent event )
{
}

void AnotherSandboxProjectApp::mouseDown( MouseEvent event )
{
}

void AnotherSandboxProjectApp::update()
{
#if !FIXEDBUFFER
    _DSPController->generateSamples();
#endif
}

void AnotherSandboxProjectApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 
}

CINDER_APP( AnotherSandboxProjectApp, RendererGl )
