#include "NPRelicReturnVisualBlueprintBuilder.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/StaticMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Gameplay/Relic/NPRelicReturnVisualLibrary.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
    constexpr TCHAR BlueprintObjectPath[] = TEXT("/Game/NoPhotos/Blueprints/MapEvent/RelicReturnBonus/BP_RelicReturnVisual.BP_RelicReturnVisual");
    constexpr TCHAR GeneratedComment[] = TEXT("[GeneratedRelicReturnVisual]");
    const FName VisualMeshVariableName(TEXT("VisualMesh"));

    template <typename NodeType>
    NodeType* AddNode(UEdGraph* Graph, const int32 X, const int32 Y)
    {
        NodeType* Node = NewObject<NodeType>(Graph);
        Graph->AddNode(Node, true, false);
        Node->CreateNewGuid();
        Node->NodePosX = X;
        Node->NodePosY = Y;
        Node->NodeComment = GeneratedComment;
        Node->bCommentBubbleVisible = false;
        Node->PostPlacedNewNode();
        return Node;
    }

    UEdGraphPin* FindPinChecked(UEdGraphNode* Node, const FName PinName)
    {
        UEdGraphPin* Pin = Node ? Node->FindPin(PinName) : nullptr;
        checkf(Pin, TEXT("Generated node %s is missing pin %s"), *GetNameSafe(Node), *PinName.ToString());
        return Pin;
    }
}

bool UNPRelicReturnVisualBlueprintBuilder::BuildRelicReturnVisualBlueprint()
{
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, BlueprintObjectPath);
    if (!IsValid(Blueprint) || !IsValid(Blueprint->SimpleConstructionScript))
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load %s"), BlueprintObjectPath);
        return false;
    }

    Blueprint->Modify();

    USCS_Node* VisualMeshNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (IsValid(Node) && Node->GetVariableName() == VisualMeshVariableName)
        {
            VisualMeshNode = Node;
            break;
        }
    }

    if (!VisualMeshNode)
    {
        VisualMeshNode = Blueprint->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(), VisualMeshVariableName);
        Blueprint->SimpleConstructionScript->AddNode(VisualMeshNode);
    }

    if (UStaticMeshComponent* MeshTemplate = Cast<UStaticMeshComponent>(VisualMeshNode->ComponentTemplate))
    {
        MeshTemplate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MeshTemplate->SetGenerateOverlapEvents(false);
        MeshTemplate->SetSimulatePhysics(false);
    }

    UEdGraph* EventGraph = Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
    if (!IsValid(EventGraph))
    {
        UE_LOG(LogTemp, Error, TEXT("%s has no event graph"), BlueprintObjectPath);
        return false;
    }

    TArray<UEdGraphNode*> NodesToRemove;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (IsValid(Node) && Node->NodeComment == GeneratedComment)
        {
            NodesToRemove.Add(Node);
        }
        else if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
        {
            if (CustomEvent->CustomFunctionName == TEXT("InitializeReturnVisual"))
            {
                NodesToRemove.Add(Node);
            }
        }
    }
    for (UEdGraphNode* Node : NodesToRemove)
    {
        EventGraph->RemoveNode(Node);
    }

    UK2Node_CustomEvent* InitializeEvent = AddNode<UK2Node_CustomEvent>(EventGraph, 0, 0);
    InitializeEvent->CustomFunctionName = TEXT("InitializeReturnVisual");
    InitializeEvent->AllocateDefaultPins();

    FEdGraphPinType MeshPinType;
    MeshPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
    MeshPinType.PinSubCategoryObject = UStaticMeshComponent::StaticClass();
    InitializeEvent->CreateUserDefinedPin(TEXT("SourceMeshComponent"), MeshPinType, EGPD_Output);

    FEdGraphPinType VectorPinType;
    VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    InitializeEvent->CreateUserDefinedPin(TEXT("StartLocation"), VectorPinType, EGPD_Output);
    InitializeEvent->CreateUserDefinedPin(TEXT("TargetLocation"), VectorPinType, EGPD_Output);

    FEdGraphPinType FloatPinType;
    FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    InitializeEvent->CreateUserDefinedPin(TEXT("Duration"), FloatPinType, EGPD_Output);

    UK2Node_VariableGet* VisualMeshGet = AddNode<UK2Node_VariableGet>(EventGraph, 330, 260);
    VisualMeshGet->VariableReference.SetSelfMember(VisualMeshVariableName);
    VisualMeshGet->AllocateDefaultPins();

    UK2Node_CallFunction* ConfigureAndAnimate = AddNode<UK2Node_CallFunction>(EventGraph, 620, 0);
    const UFunction* RuntimeFunction = UNPRelicReturnVisualLibrary::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UNPRelicReturnVisualLibrary, ConfigureAndAnimateRelicReturnVisual));
    if (!RuntimeFunction)
    {
        UE_LOG(LogTemp, Error, TEXT("ConfigureAndAnimateRelicReturnVisual was not found"));
        return false;
    }
    ConfigureAndAnimate->SetFromFunction(RuntimeFunction);
    ConfigureAndAnimate->AllocateDefaultPins();

    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    Schema->TryCreateConnection(
        FindPinChecked(InitializeEvent, UEdGraphSchema_K2::PN_Then),
        FindPinChecked(ConfigureAndAnimate, UEdGraphSchema_K2::PN_Execute));
    Schema->TryCreateConnection(
        FindPinChecked(VisualMeshGet, VisualMeshVariableName),
        FindPinChecked(ConfigureAndAnimate, TEXT("VisualMesh")));
    Schema->TryCreateConnection(
        FindPinChecked(InitializeEvent, TEXT("SourceMeshComponent")),
        FindPinChecked(ConfigureAndAnimate, TEXT("SourceMeshComponent")));
    Schema->TryCreateConnection(
        FindPinChecked(InitializeEvent, TEXT("StartLocation")),
        FindPinChecked(ConfigureAndAnimate, TEXT("StartLocation")));
    Schema->TryCreateConnection(
        FindPinChecked(InitializeEvent, TEXT("TargetLocation")),
        FindPinChecked(ConfigureAndAnimate, TEXT("TargetLocation")));
    Schema->TryCreateConnection(
        FindPinChecked(InitializeEvent, TEXT("Duration")),
        FindPinChecked(ConfigureAndAnimate, TEXT("Duration")));

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    if (Blueprint->Status == BS_Error)
    {
        UE_LOG(LogTemp, Error, TEXT("Generated blueprint failed to compile: %s"), BlueprintObjectPath);
        return false;
    }

    UPackage* Package = Blueprint->GetOutermost();
    Package->MarkPackageDirty();
    const FString PackageFilename = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    const bool bSaved = UPackage::SavePackage(Package, Blueprint, *PackageFilename, SaveArgs);
    UE_LOG(LogTemp, Display, TEXT("BP_RelicReturnVisual generation %s"), bSaved ? TEXT("succeeded") : TEXT("failed"));
    return bSaved;
}
