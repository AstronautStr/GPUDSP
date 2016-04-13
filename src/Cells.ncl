#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable

float waveTableOsc(__global float* waveTable, float frequency, uint sampleRate, uint samplesProcessed, uint samplePosition);
uint ringIndex(int index, uint size);
uint2 torIndex(int2 index, uint2 size);
float rand(float2 co);
float randFreq(float fractTime);

float waveTableOsc(__global float* waveTable, float frequency, uint sampleRate, uint samplesProcessed, uint samplePosition)
{
    uint     frecInt     = uint(frequency);
    float    frecFract;  fract(frequency, &frecFract);
    
    uint     SPmSR       = samplesProcessed % sampleRate;
    uint     FImSR       = frecInt % sampleRate;
    uint     SImSR       = samplePosition % sampleRate;
    uint     intPart     = (SPmSR * FImSR) % sampleRate + (SImSR * FImSR) % sampleRate;
    uint     fractPart   = uint(float(samplesProcessed) * frecFract) % sampleRate + uint(float(samplePosition) * frecFract) % sampleRate;
    
    uint     tableIndex  = (intPart + fractPart) % sampleRate;
    
    return waveTable[tableIndex];
}

uint ringIndex(int index, uint size)
{
    return (size + index) % size;
}

uint2 torIndex(int2 index, uint2 size)
{
    return (uint2)(ringIndex(index.x, size.x), ringIndex(index.y, size.y));
}


float rand(float2 co)
{
    float value = 0.0f;
    return fract(sin(dot(co.xy, (float2)(12.9898f, 78.233f))) * 43758.5453f, &value);
}

float randFreq(float fractTime)
{
    float min = 20.0f;
    float max = 22000.0f;
    return pow(2.0f, (log2(min) + (log2(max) - log2(min)) * rand((float2)(fractTime, 1.3217f))));
}

__kernel void kernelMain(__global float* samples, __global float* waveTable, uint sampleRate, uint samplesProcessed, uint bufferSize, __global float4* cells, __global float* rules, uint2 gridSize)
{
    // cells is gridSize.x * gridSize.y * bufferSize length
    uint globalID = get_global_id(0);
    
    uint2 cellPosition = (uint2)(globalID / gridSize.y, globalID % gridSize.y);
    if (cellPosition.x >= gridSize.x || cellPosition.y >= gridSize.y)
        return;
    
    for (uint sampleIdx = 0; sampleIdx < bufferSize; ++sampleIdx)
    {
        globalID = get_global_id(0);
        uint cellIndex = sampleIdx * gridSize.x * gridSize.y + globalID;
        
        float4 cell = cells[cellIndex];
        float signal = cell.x * waveTableOsc(waveTable, cell.y, sampleRate, samplesProcessed, sampleIdx);
        cells[cellIndex].z = signal;
        
        barrier(CLK_GLOBAL_MEM_FENCE);
        
        globalID = get_global_id(0);
        cellIndex = sampleIdx * gridSize.x * gridSize.y + globalID;

        cell = cells[cellIndex];
        cellPosition = (uint2)(globalID / gridSize.y, globalID % gridSize.y);
        
        float sum = 0.0f;
        int ruleRadius = 1;
        for (int i = -ruleRadius; i <= ruleRadius; ++i)
        {
            for (int j = - ruleRadius; j <= ruleRadius; j++)
            {
                if (i == 0 && j == 0)
                    continue;
                
                uint2 broIdx = torIndex((int2)cellPosition + (int2)(i, j), gridSize);
                float4 bro = cells[(broIdx.x * gridSize.y + broIdx.y) + sampleIdx * gridSize.x * gridSize.y];
                
                sum += bro.x * bro.z;
            }
        }
        
        float deltaSign = -1.0f;
        float deltaValue = rules[4] / (float)sampleRate;
        float rulesBirthCenter = rules[0];
        float rulesBirthRadius = rules[1];
        float rulesKeepCenter = rules[2];
        float rulesKeepRadius = rules[3];
        
        if (sum >= rulesBirthCenter - rulesBirthRadius && sum <= rulesBirthCenter + rulesBirthRadius)
            deltaSign = 1.0f;
        else if (sum >= rulesKeepCenter - rulesKeepRadius && sum <= rulesKeepCenter + rulesKeepRadius)
            deltaSign = 0.0f;
        
        int cellNextStepIndex = globalID + ((sampleIdx + 1) % bufferSize) * gridSize.x * gridSize.y;

        float newAmp = clamp(cell.x + deltaValue * deltaSign, 0.0f, 1.0f);
        float randFreqMask = floor(1.0f - newAmp);
        cells[cellNextStepIndex].x = newAmp;
        cells[cellNextStepIndex].y = cell.y * (1 - randFreqMask) + randFreq((float)((samplesProcessed % sampleRate + sampleIdx) % sampleRate) / (float)sampleRate) * randFreqMask;
        
        barrier(CLK_GLOBAL_MEM_FENCE);
    }
}