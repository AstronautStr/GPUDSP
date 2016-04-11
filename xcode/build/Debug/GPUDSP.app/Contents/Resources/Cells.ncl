#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable

float waveTableOsc(__global float* waveTable, float frequency, uint sampleRate, uint samplesProcessed, uint samplePosition);
uint ringIndex(int index, uint size);
uint2 torIndex(int2 index, uint2 size);

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

__kernel void kernelMain(__global float* samples, __global float* waveTable, uint sampleRate, uint samplesProcessed, uint bufferSize, __global int* mutex, __global float4* cells, __global float4* cellsNew, uint2 gridSize)
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
        float deltaValue = 1.0f / (float)sampleRate;
        const float rulesBirthCenter = rand((float2)(0.1f, (float)(samplesProcessed / sampleRate)));
        const float rulesBirthRadius = rand((float2)(0.35f, (float)(samplesProcessed / sampleRate)));
        const float rulesKeepCenter = rand((float2)(0.7f, (float)(samplesProcessed / sampleRate)));
        const float rulesKeepRadius = rand((float2)(0.55f, (float)(samplesProcessed / sampleRate)));
        
        if (sum >= rulesBirthCenter - rulesBirthRadius && sum <= rulesBirthCenter + rulesBirthRadius)
            deltaSign = 1.0f;
        else if (sum >= rulesKeepCenter - rulesKeepRadius && sum <= rulesKeepCenter + rulesKeepRadius)
            deltaSign = 0.0f;
        
        int cellNextStepIndex = globalID + ((sampleIdx + 1) % bufferSize) * gridSize.x * gridSize.y;

        cells[cellNextStepIndex].x = clamp(cell.x + deltaValue * deltaSign, 0.0f, 1.0f);
        cells[cellNextStepIndex].y = cell.y;
    }
}