struct PSIn
{
    float4 pos   : SV_Position;
    float  bright : COLOR0;
};

float4 main(PSIn i) : SV_Target
{
    return float4(i.bright, i.bright, i.bright, 1.0);
}
