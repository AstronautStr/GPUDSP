void osc(__global float* samples, __global float* waveTable, uint sampleRate, uint samplesProcessed, float frequency)
{
    uint samplePosition = get_global_id(0);

    uint     frecInt     = uint(frequency);
    float    frecFract;  fract(frequency, &frecFract);
    
    uint     SPmSR       = samplesProcessed % sampleRate;
    uint     FImSR       = frecInt % sampleRate;
    uint     SImSR       = samplePosition % sampleRate;
    uint     intPart     = (SPmSR * FImSR) % sampleRate + (SImSR * FImSR) % sampleRate;
    uint     fractPart   = uint(float(samplesProcessed) * frecFract) % sampleRate + uint(float(samplePosition) * frecFract) % sampleRate;
    
    uint     tableIndex  = (intPart + fractPart) % sampleRate;
    
    samples[samplePosition] = waveTable[tableIndex];
}

__kernel void kernelMain(__global float* samples, __global float* waveTable, uint sampleRate, uint samplesProcessed, uint bufferSize, __global int* mutex, __global float4* cells, __global float4* cellsNew, uint2 gridSize)
{
    /*
    uint samplePosition = get_global_id(0);
    uint size = gridSize.x * gridSize.y;

    float freq = cells[samplePosition * size].y;
    osc(samples, waveTable, sampleRate, samplesProcessed, freq);
    */
    uint globalID = get_global_id(0);
    uint size = gridSize.x * gridSize.y;
    
    samples[globalID] = 0.0f;
    float power = 0.0f;
    for (int i = 0; i < size; ++i)
    {
        power += fabs(cells[i + globalID * size].x);
        samples[globalID] += cells[i + globalID * size].z;
    }
     
    samples[globalID] = samples[globalID] / power;
}