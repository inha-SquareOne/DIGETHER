#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DigetherSoilField.h"
#include <limits>

using namespace DigetherSoil;
namespace
{
bool ClosedSurface(const FSurface& S)
{
	// Weld positions for this test: renderer vertices are intentionally split per face.
	TMap<FString, int32> Edges;
	for (int32 I = 0; I < S.Indices.Num(); I += 3)
	{
		for (int32 J = 0; J < 3; ++J)
		{
			const FVector A = S.Vertices[S.Indices[I + J]], B = S.Vertices[S.Indices[I + (J+1)%3]];
			FString KA = FString::Printf(TEXT("%.5f,%.5f,%.5f"), A.X,A.Y,A.Z);
			FString KB = FString::Printf(TEXT("%.5f,%.5f,%.5f"), B.X,B.Y,B.Z);
			if (KA > KB) { Swap(KA,KB); }
			++Edges.FindOrAdd(KA + TEXT("/") + KB);
		}
	}
	for (const auto& Edge : Edges) { if (Edge.Value != 2) { return false; } }
	return !Edges.IsEmpty();
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoilTopologyTest,"DIGETHER.Dain.SoilCore.Topology",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoilTopologyTest::RunTest(const FString& Parameters)
{
	FGrid G;
	TArray<uint8> D;
	TestTrue(TEXT("initialize"),Initialize(G,D));
	FSurface S;
	if (!TestTrue(TEXT("closed volume builds"),BuildSurface(G,D,S))) { return false; }
	TestTrue(TEXT("every geometric edge has two faces"),ClosedSurface(S));
	FBox Bounds(ForceInit);
	for (const FVector& P : S.Vertices) { Bounds += P; }
	TestTrue(TEXT("all six faces close inside sample boundary"),Bounds.Min.Equals(FVector(25),0.01) && Bounds.Max.Equals(FVector(375),0.01));
	bool bWinding = true;
	for (int32 I=0; I<S.Indices.Num(); I+=3)
	{
		const FVector A=S.Vertices[I],B=S.Vertices[I+1],C=S.Vertices[I+2];
		bWinding &= FVector::DotProduct(FVector::CrossProduct(B-A,C-A),S.Normals[I]) < -0.0001;
		bWinding &= FVector::DotProduct((A+B+C)/3-FVector(200),S.Normals[I]) > 0;
	}
	TestTrue(TEXT("outward normals and UE reversed winding"),bWinding);
	FBox Changed;
	TestTrue(TEXT("cut side"),Carve(G,D,FVector(25,200,200),100,{},Changed)==ECarveResult::Changed);
	TestTrue(TEXT("cut surface builds"),BuildSurface(G,D,S));
	TestTrue(TEXT("cut remains watertight"),ClosedSurface(S));
    // Shared positions must have matching lighting normals, even with split render vertices.
    TMap<FString,FVector> SharedNormals;
    bool bSmooth = true;
    for (int32 I=0; I<S.Vertices.Num(); ++I)
    {
        const FVector P=S.Vertices[I];
        const FString Key=FString::Printf(TEXT("%.5f,%.5f,%.5f"),P.X,P.Y,P.Z);
        if (const FVector* Previous=SharedNormals.Find(Key)) { bSmooth &= Previous->Equals(S.Normals[I],0.001); }
        else { SharedNormals.Add(Key,S.Normals[I]); }
        bSmooth &= !S.Normals[I].ContainsNaN() && FMath::IsNearlyEqual(S.Normals[I].Size(),1.0,0.001);
    }
    TestTrue(TEXT("continuous unit normals across triangle boundaries"),bSmooth);
	D.Init(1,G.Count());
	TestTrue(TEXT("empty is a valid surface"),BuildSurface(G,D,S));
	TestEqual(TEXT("empty replaces previous geometry"),S.Indices.Num(),0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoilCarveTest,"DIGETHER.Dain.SoilCore.Carve",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoilCarveTest::RunTest(const FString& Parameters)
{
	FGrid G;
	TArray<uint8> Initial; Initialize(G,Initial);
	TArray<uint8> D=Initial, Peer=Initial;
	FBox Changed, Other;
	const FVector Center(200);
	TestTrue(TEXT("interior cut changes"),Carve(G,D,Center,100,{},Changed)==ECarveResult::Changed);
	TestTrue(TEXT("center becomes air"),D[G.Index(4,4,4)]<128);
	Carve(G,Peer,Center,100,{},Other);
	TestTrue(TEXT("same input same density"),D==Peer);
	TestTrue(TEXT("same brush is idempotent"),Carve(G,D,Center,100,{},Other)==ECarveResult::NoChange);
	TestFalse(TEXT("no change has no bounds"),bool(Other.IsValid));
	bool bBounds=true, bBoundary=true;
	for(int32 Z=0;Z<=8;++Z) for(int32 Y=0;Y<=8;++Y) for(int32 X=0;X<=8;++X)
	{
		const int32 I=G.Index(X,Y,Z);
		if(D[I]!=Initial[I]) { bBounds &= Changed.IsInsideOrOn(G.Position(X,Y,Z)); }
		if(X==0||Y==0||Z==0||X==8||Y==8||Z==8) { bBoundary &= D[I]==Initial[I]; }
	}
	TestTrue(TEXT("changed bounds enclose changes"),bBounds);
	TestTrue(TEXT("outer air unchanged"),bBoundary);
	D=Initial;
	const FBox Protected(FVector(175),FVector(225));
	TestTrue(TEXT("partial protected carve"),Carve(G,D,Center,300,{Protected},Changed)==ECarveResult::Changed);
	bool bProtected=true;
	for(int32 Z=0;Z<=8;++Z) for(int32 Y=0;Y<=8;++Y) for(int32 X=0;X<=8;++X)
		if(Protected.ExpandBy(G.Spacing).IsInsideOrOn(G.Position(X,Y,Z))) { bProtected &= D[G.Index(X,Y,Z)]==Initial[G.Index(X,Y,Z)]; }
	TestTrue(TEXT("protected samples and one-cell halo unchanged"),bProtected);
	D=Initial;
	TestTrue(TEXT("all protected means no change"),Carve(G,D,Center,300,{FBox(FVector::ZeroVector,FVector(400))},Changed)==ECarveResult::NoChange);
	TestTrue(TEXT("full carve"),Carve(G,D,Center,600,{},Changed)==ECarveResult::Changed);
	FSurface S;
	TestTrue(TEXT("full removal succeeds"),BuildSurface(G,D,S));
	TestEqual(TEXT("no triangles after removal"),S.Indices.Num(),0);
	for(const FRotator Rotation : {FRotator::ZeroRotator,FRotator(90,0,0),FRotator(180,0,0)})
	{
		const FTransform T(Rotation,FVector(1000,500,-100));
		FVector Local;
		TestTrue(TEXT("rigid transform accepted"),WorldToLocal(T,T.TransformPosition(Center),Local));
		Peer=Initial; Carve(G,Peer,Local,100,{},Other);
		TArray<uint8> Reference=Initial; Carve(G,Reference,Center,100,{},Other);
		TestTrue(TEXT("floor wall ceiling same local result"),Peer==Reference);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoilValidationTest,"DIGETHER.Dain.SoilCore.Validation",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoilValidationTest::RunTest(const FString& Parameters)
{
	FGrid G; TArray<uint8> D; Initialize(G,D); const TArray<uint8> Before=D;
	FBox Changed;
	const double NaN=std::numeric_limits<double>::quiet_NaN();
	for(double Radius : {NaN, std::numeric_limits<double>::infinity(), -1.0, 0.0, 1.e20})
		TestTrue(TEXT("invalid radius rejected"),Carve(G,D,FVector(200),Radius,{},Changed)==ECarveResult::InvalidInput);
	TestTrue(TEXT("NaN point rejected"),Carve(G,D,FVector(NaN,0,0),100,{},Changed)==ECarveResult::InvalidInput);
	TestTrue(TEXT("outside point rejected"),Carve(G,D,FVector(-1,0,0),100,{},Changed)==ECarveResult::InvalidInput);
	TestTrue(TEXT("invalid protection rejected"),Carve(G,D,FVector(200),100,{FBox(ForceInit)},Changed)==ECarveResult::InvalidInput);
	TestTrue(TEXT("rejection preserves data"),D==Before);
	FVector Local(7);
	TestFalse(TEXT("scaled actor rejected"),WorldToLocal(FTransform(FQuat::Identity,FVector::ZeroVector,FVector(2,1,1)),FVector(200),Local));
	TestTrue(TEXT("failed conversion preserves output"),Local==FVector(7));
	D.Pop(); FSurface S; S.Vertices.Add(FVector(7));
	TestFalse(TEXT("wrong array length rejected"),BuildSurface(G,D,S));
	TestEqual(TEXT("failed build preserves previous surface"),S.Vertices.Num(),1);
	D=Before;D[0]=255;
	TestFalse(TEXT("solid exterior rejected"),BuildSurface(G,D,S));
	G.Cells.X=MAX_int32;
	TestFalse(TEXT("oversized grid rejected before allocation"),Initialize(G,D));
	G=FGrid(); G.Spacing=NaN;
	TestFalse(TEXT("invalid spacing rejected"),Initialize(G,D));
	TestTrue(TEXT("invalid carve grid rejected before geometry math"),Carve(G,D,FVector(200),100,{},Changed)==ECarveResult::InvalidInput);
	return true;
}
#endif


