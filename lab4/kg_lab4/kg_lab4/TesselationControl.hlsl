struct PatchTess
{
    float OuterTess[4] : SV_TessFactor;
    float InnerTess[2] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 4>)