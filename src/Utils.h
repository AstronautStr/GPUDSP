//
//  Utils.h
//  GPUDSP
//
//  Created by Ilya Solovyov on 13/04/16.
//
//

#ifndef Utils_h
#define Utils_h

std::string readAllText(std::string const& path)
{
    std::ifstream ifstr(path);
    if (!ifstr)
        return std::string();
    std::stringstream text;
    ifstr >> text.rdbuf();
    return text.str();
}

typedef std::basic_string<GLchar>  GLstring;

GLstring lastError(GLuint shader)
{
    GLint logLen{ 0 };
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
    std::vector<GLchar> log;
    log.resize(logLen, '\0');
    glGetShaderInfoLog(shader, log.size(), nullptr, log.data());
    
    return GLstring(std::begin(log), std::end(log));
}

float randAmp()
{
    return (float)rand() / RAND_MAX;
}

float randFreq(float min = 80.0f, float max = 10000.0f, bool white = true)
{
    if (white)
        return pow(2.0, (log2(min) + (log2(max) - log2(min)) * ((float)rand() / RAND_MAX)));
    else
        return 60.0f - 0.25f + 0.5f * ((float)rand() / RAND_MAX);
}

#endif /* Utils_h */
