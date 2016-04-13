//
//  DSPOpenGL.h
//  AnotherSandboxProject
//
//  Created by Ilya Solovyov on 08.04.16.
//
//

#ifndef DSPOpenGL_h
#define DSPOpenGL_h

#include "Utils.h"
#include "cinder/gl/gl.h"
#include "cinder/app/cocoa/PlatformCocoa.h"
#include "cinder/audio/audio.h"
#include "cinder/audio/dsp/Dsp.h"

typedef GLfloat                     SampleValueType;

#if UNSAFEBUFFER
#include "UnsafeRingBuffer.h"
typedef UnsafeRingBufferT<SampleValueType> RingBuffer;
#else
#include "cinder/audio/audio.h"
#include "cinder/audio/dsp/Dsp.h"
typedef ci::audio::dsp::RingBufferT<SampleValueType> RingBuffer;
#endif

class DSPOpenGL
{
public:
    RingBuffer RingBuffer;
    
private:
    GLuint _nodesVBO;
    GLuint _nodesVAO;
    GLuint _waveTableBuffer;
    GLuint _waveTableTex;
    GLuint _DSPFeedbackBuffer;
    SampleValueType* _feedbackData;
    
    GLuint _DSPShader;
    GLuint _DSPProgram;
    GLuint _samplesProcessedUniformLoc;
    GLuint _waveTableSamplerLoc;
    
    GLuint _samplesTex;
    
    size_t _samplesProcessed;
    size_t _sampleRate;
    
    typedef std::basic_string<GLchar>   GLstring;
    
    GLstring lastError(GLuint shader)
    {
        GLint logLen{ 0 };
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<GLchar> log;
        log.resize(logLen, '\0');
        glGetShaderInfoLog(shader, log.size(), nullptr, log.data());
        
        return GLstring(std::begin(log), std::end(log));
    }
    
    void _prepareSoundFeedback()
    {
        _prepareSoundFeedbackProgram();
        _prepareSoundFeedbackBuffers();
        _prepareSoundFeedbackVertexArray();
    }
    void _prepareSoundFeedbackProgram()
    {
        std::string vertexShaderSrcString = readAllText(PlatformCocoa::get()->getResourcePath("GPUDSP.vert").string());
        GLchar const* const vertexShaderSrc = vertexShaderSrcString.c_str();
        
        _DSPShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(_DSPShader, 1, &vertexShaderSrc, nullptr);
        glCompileShader(_DSPShader);
        
        std::cerr << lastError(_DSPShader) << std::endl;
        
        _DSPProgram = glCreateProgram();
        glAttachShader(_DSPProgram, _DSPShader);
        
        const GLchar* feedbackVaryings[] = { "sampleValue" };
        glTransformFeedbackVaryings(_DSPProgram, 1, feedbackVaryings, GL_INTERLEAVED_ATTRIBS);
        
        glLinkProgram(_DSPProgram);
        std::cerr << lastError(_DSPProgram) << std::endl;
        
        GLint sampleRateLoc = glGetUniformLocation(_DSPProgram, "sampleRate");
        glProgramUniform1i(_DSPProgram, sampleRateLoc, _sampleRate);
        
        _samplesProcessedUniformLoc = glGetUniformLocation(_DSPProgram, "samplesProcessed");
        _waveTableSamplerLoc = glGetUniformLocation(_DSPProgram, "waveTable");
    }
    void _prepareSoundFeedbackBuffers()
    {
        size_t nodesCount = getBufferSize();
        size_t _samplesBytesSize = nodesCount * sizeof(SampleValueType);
        _feedbackData = new SampleValueType[nodesCount];
        
        GLint* sampleIndexData = new GLint[nodesCount];
        int sampleIndexDataBytesSize = nodesCount * sizeof(GLint);
        for (size_t i = 0; i < nodesCount; ++i)
        {
            sampleIndexData[i] = i;
            _feedbackData[i] = 0;
        }
        glGenBuffers(1, &_DSPFeedbackBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, _DSPFeedbackBuffer);
        glBufferData(GL_ARRAY_BUFFER, _samplesBytesSize, _feedbackData, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        glGenBuffers(1, &_nodesVBO);
        glBindBuffer(GL_ARRAY_BUFFER, _nodesVBO);
        glBufferData(GL_ARRAY_BUFFER, sampleIndexDataBytesSize, sampleIndexData, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        
        SampleValueType* waveTable = new SampleValueType[_sampleRate];
        size_t waveTableBytesSize = _sampleRate * sizeof(SampleValueType);
        for (size_t i = 0; i < _sampleRate; ++i)
        {
            waveTable[i] = (SampleValueType)cos(2 * M_PI * (double)i / (double)_sampleRate);
        }
        
        glGenBuffers(1, &_waveTableBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, _waveTableBuffer);
        glBufferData(GL_ARRAY_BUFFER, waveTableBytesSize, waveTable, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        delete [] waveTable;
        
        glGenTextures(1, &_waveTableTex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, _waveTableTex);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, _waveTableBuffer);
        glBindTexture(GL_TEXTURE_BUFFER, 0);
        
    }
    void _prepareSoundFeedbackVertexArray()
    {
        glGenVertexArrays(1, &_nodesVAO);
        glBindVertexArray(_nodesVAO);
        
        glBindBuffer(GL_ARRAY_BUFFER, _nodesVBO);
        GLint samplePositionAttr = glGetAttribLocation(_DSPProgram, "samplePosition");
        glVertexAttribIPointer(samplePositionAttr, 1, GL_INT, 0, NULL);
        glEnableVertexAttribArray(samplePositionAttr);
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    
    void _generateNodes()
    {
        _prepareSoundFeedbackProgram();
        _prepareSoundFeedbackBuffers();
        _prepareSoundFeedbackVertexArray();
    }
    
public:
    DSPOpenGL(size_t sampleRate, size_t bufferSize) :
    RingBuffer(bufferSize)
    {
        _samplesProcessed = 0;
        _sampleRate = sampleRate;
        _generateNodes();
    }
    
    ~DSPOpenGL()
    {
        delete [] _feedbackData;
        
        glDeleteProgram(_DSPProgram);
        glDeleteShader(_DSPShader);
        
        glDeleteBuffers(1, &_nodesVBO);
        glDeleteBuffers(1, &_DSPFeedbackBuffer);
        glDeleteBuffers(1, &_waveTableBuffer);
        glDeleteTextures(1, &_waveTableTex);
        
        glDeleteVertexArrays(1, &_nodesVAO);
    }
    
    size_t getBufferSize()
    {
        return RingBuffer.getSize();
    }
    
    void generateSamples()
    {
        size_t toWrite = RingBuffer.getAvailableWrite();
        
        if (toWrite <= 0)
            return;
        
        glEnable(GL_RASTERIZER_DISCARD);
        glUseProgram(_DSPProgram);
        
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, _DSPFeedbackBuffer);
        glBindVertexArray(_nodesVAO);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, _waveTableTex);
        glUniform1i(_waveTableSamplerLoc, 0);
        
        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, toWrite);
        glEndTransformFeedback();
        
        glFlush();
        
        glBindVertexArray(0);
        glDisable(GL_RASTERIZER_DISCARD);
        glUseProgram(0);
        
        // recieve processed data from feedback
#if UNSAFEBUFFER
        SampleValueType* firstPart = nullptr;
        SampleValueType* secondPart = nullptr;
        size_t firstLength = 0;
        size_t secondLength = 0;
        
        RingBuffer.getUnsafeDataWritePointer(toWrite, firstPart, secondPart, firstLength, secondLength);
        if (firstPart != nullptr)
        {
            glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, firstLength * sizeof(SampleValueType), firstPart);
            _samplesProcessed += firstLength;
            if (secondPart != nullptr)
            {
                glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, firstLength * sizeof(SampleValueType), secondLength * sizeof(SampleValueType), secondPart);
                _samplesProcessed += secondLength;
            }
            
        }
#else
        glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, toWrite * sizeof(SampleValueType), _feedbackData);
        RingBuffer.write(_feedbackData, toWrite);
        _samplesProcessed += toWrite;
#endif
        
#if LOGENABLED
        std::cerr << "[ProcessingThread]: processed " << toWrite << "samples" << std::endl;
#endif
        glProgramUniform1i(_DSPProgram, _samplesProcessedUniformLoc, _samplesProcessed);
    }
};

#endif /* DSPOpenGL_h */
