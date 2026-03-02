#include "Commands/UnrealMCPBlueprintCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "MaterialEditingLibrary.h"
#include "Components/MeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Misc/PackageName.h"

namespace
{
    FString NormalizeAssetPath(const FString& InPath, const FString& DefaultRoot)
    {
        FString Path = InPath;
        if (Path.IsEmpty())
        {
            Path = DefaultRoot;
        }

        if (!Path.StartsWith(TEXT("/")))
        {
            Path = DefaultRoot / Path;
        }

        return Path;
    }

    FString NormalizeObjectPath(const FString& InPath, const FString& DefaultRoot)
    {
        FString Path = NormalizeAssetPath(InPath, DefaultRoot);
        if (!Path.Contains(TEXT(".")))
        {
            const FString ShortName = FPackageName::GetShortName(Path);
            Path = FString::Printf(TEXT("%s.%s"), *Path, *ShortName);
        }
        return Path;
    }

    UMaterialExpression* AddExpr(UMaterial* Material, TSubclassOf<UMaterialExpression> ExprClass, int32 X, int32 Y)
    {
        return UMaterialEditingLibrary::CreateMaterialExpression(Material, ExprClass, X, Y);
    }

    void ConfigurePresetDefaults(UMaterialInstanceConstant* Instance, const FString& Preset, int32 VariantIndex)
    {
        const float T = FMath::Clamp(static_cast<float>(VariantIndex) / 9.0f, 0.0f, 1.0f);

        if (Preset == TEXT("toon"))
        {
            UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("BaseColor"), FLinearColor(0.2f + 0.6f * T, 0.3f + 0.3f * T, 0.8f - 0.5f * T, 1.0f));
            UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("OutlineColor"), FLinearColor::Black);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("OutlineStrength"), 0.1f + 0.8f * T);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Roughness"), 0.35f);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Metallic"), 0.0f);
        }
        else if (Preset == TEXT("hatch"))
        {
            UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("BaseColor"), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
            UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("HatchColor"), FLinearColor::Black);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("HatchDensity"), 6.0f + 40.0f * T);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("HatchStrength"), 0.1f + 0.85f * T);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Roughness"), 0.8f);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Metallic"), 0.0f);
        }
        else if (Preset == TEXT("emissive"))
        {
            UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("BaseColor"), FLinearColor(0.04f, 0.04f, 0.04f, 1.0f));
            UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("EmissiveColor"), FLinearColor(0.1f + 0.9f * T, 0.2f + 0.8f * (1.0f - T), 1.0f, 1.0f));
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("EmissiveStrength"), 0.5f + 18.0f * T);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Roughness"), 0.2f);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Metallic"), 0.0f);
        }
        else if (Preset == TEXT("water"))
        {
            UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("BaseColor"), FLinearColor(0.0f, 0.18f + 0.4f * T, 0.45f + 0.4f * T, 1.0f));
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Opacity"), 0.05f + 0.4f * T);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Roughness"), 0.02f + 0.2f * (1.0f - T));
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Metallic"), 0.0f);
            UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("EmissiveColor"), FLinearColor(0.0f, 0.04f, 0.08f, 1.0f));
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("EmissiveStrength"), 0.2f + 0.4f * T);
        }
        else if (Preset == TEXT("ice"))
        {
            UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("BaseColor"), FLinearColor(0.6f + 0.3f * T, 0.75f + 0.2f * T, 1.0f, 1.0f));
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Opacity"), 0.15f + 0.55f * T);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Roughness"), 0.02f + 0.3f * T);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Metallic"), 0.0f);
            UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("EmissiveColor"), FLinearColor(0.0f, 0.02f, 0.08f, 1.0f));
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("EmissiveStrength"), 0.1f + 0.3f * (1.0f - T));
        }
        else if (Preset == TEXT("rm"))
        {
            UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, TEXT("BaseColor"), FLinearColor(0.9f, 0.65f, 0.2f, 1.0f));
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Roughness"), T);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("Metallic"), 1.0f - T);
            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, TEXT("EmissiveStrength"), 0.0f);
        }
    }
}

FUnrealMCPBlueprintCommands::FUnrealMCPBlueprintCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_blueprint"))
    {
        return HandleCreateBlueprint(Params);
    }
    else if (CommandType == TEXT("add_component_to_blueprint"))
    {
        return HandleAddComponentToBlueprint(Params);
    }
    else if (CommandType == TEXT("create_material"))
    {
        return HandleCreateMaterial(Params);
    }
    else if (CommandType == TEXT("create_material_instance"))
    {
        return HandleCreateMaterialInstance(Params);
    }
    else if (CommandType == TEXT("set_material_param"))
    {
        return HandleSetMaterialParam(Params);
    }
    else if (CommandType == TEXT("assign_material"))
    {
        return HandleAssignMaterial(Params);
    }
    else if (CommandType == TEXT("set_component_property"))
    {
        return HandleSetComponentProperty(Params);
    }
    else if (CommandType == TEXT("set_physics_properties"))
    {
        return HandleSetPhysicsProperties(Params);
    }
    else if (CommandType == TEXT("compile_blueprint"))
    {
        return HandleCompileBlueprint(Params);
    }
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    else if (CommandType == TEXT("set_blueprint_property"))
    {
        return HandleSetBlueprintProperty(Params);
    }
    else if (CommandType == TEXT("set_static_mesh_properties"))
    {
        return HandleSetStaticMeshProperties(Params);
    }
    else if (CommandType == TEXT("set_pawn_properties"))
    {
        return HandleSetPawnProperties(Params);
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Check if blueprint already exists
    FString PackagePath = TEXT("/Game/Blueprints/");
    FString AssetName = BlueprintName;
    if (UEditorAssetLibrary::DoesAssetExist(PackagePath + AssetName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint already exists: %s"), *BlueprintName));
    }

    // Create the blueprint factory
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    
    // Handle parent class
    FString ParentClass;
    Params->TryGetStringField(TEXT("parent_class"), ParentClass);
    
    // Default to Actor if no parent class specified
    UClass* SelectedParentClass = AActor::StaticClass();
    
    // Try to find the specified parent class
    if (!ParentClass.IsEmpty())
    {
        FString ClassName = ParentClass;
        if (!ClassName.StartsWith(TEXT("A")))
        {
            ClassName = TEXT("A") + ClassName;
        }
        
        // First try direct StaticClass lookup for common classes
        UClass* FoundClass = nullptr;
        if (ClassName == TEXT("APawn"))
        {
            FoundClass = APawn::StaticClass();
        }
        else if (ClassName == TEXT("AActor"))
        {
            FoundClass = AActor::StaticClass();
        }
        else
        {
            // Try loading the class using LoadClass which is more reliable than FindObject
            const FString ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
            FoundClass = LoadClass<AActor>(nullptr, *ClassPath);
            
            if (!FoundClass)
            {
                // Try alternate paths if not found
                const FString GameClassPath = FString::Printf(TEXT("/Script/Game.%s"), *ClassName);
                FoundClass = LoadClass<AActor>(nullptr, *GameClassPath);
            }
        }

        if (FoundClass)
        {
            SelectedParentClass = FoundClass;
            UE_LOG(LogTemp, Log, TEXT("Successfully set parent class to '%s'"), *ClassName);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not find specified parent class '%s' at paths: /Script/Engine.%s or /Script/Game.%s, defaulting to AActor"), 
                *ClassName, *ClassName, *ClassName);
        }
    }
    
    Factory->ParentClass = SelectedParentClass;

    // Create the blueprint
    UPackage* Package = CreatePackage(*(PackagePath + AssetName));
    UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr, GWarn));

    if (NewBlueprint)
    {
        // Notify the asset registry
        FAssetRegistryModule::AssetCreated(NewBlueprint);

        // Mark the package dirty
        Package->MarkPackageDirty();

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("name"), AssetName);
        ResultObj->SetStringField(TEXT("path"), PackagePath + AssetName);
        return ResultObj;
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create blueprint"));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleAddComponentToBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentType;
    if (!Params->TryGetStringField(TEXT("component_type"), ComponentType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Create the component - dynamically find the component class by name
    UClass* ComponentClass = nullptr;

    // Try to find the class with exact name first
    ComponentClass = FindObject<UClass>(ANY_PACKAGE, *ComponentType);
    
    // If not found, try with "Component" suffix
    if (!ComponentClass && !ComponentType.EndsWith(TEXT("Component")))
    {
        FString ComponentTypeWithSuffix = ComponentType + TEXT("Component");
        ComponentClass = FindObject<UClass>(ANY_PACKAGE, *ComponentTypeWithSuffix);
    }
    
    // If still not found, try with "U" prefix
    if (!ComponentClass && !ComponentType.StartsWith(TEXT("U")))
    {
        FString ComponentTypeWithPrefix = TEXT("U") + ComponentType;
        ComponentClass = FindObject<UClass>(ANY_PACKAGE, *ComponentTypeWithPrefix);
        
        // Try with both prefix and suffix
        if (!ComponentClass && !ComponentType.EndsWith(TEXT("Component")))
        {
            FString ComponentTypeWithBoth = TEXT("U") + ComponentType + TEXT("Component");
            ComponentClass = FindObject<UClass>(ANY_PACKAGE, *ComponentTypeWithBoth);
        }
    }
    
    // Verify that the class is a valid component type
    if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown component type: %s"), *ComponentType));
    }

    // Add the component to the blueprint
    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, *ComponentName);
    if (NewNode)
    {
        // Set transform if provided
        USceneComponent* SceneComponent = Cast<USceneComponent>(NewNode->ComponentTemplate);
        if (SceneComponent)
        {
            if (Params->HasField(TEXT("location")))
            {
                SceneComponent->SetRelativeLocation(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
            }
            if (Params->HasField(TEXT("rotation")))
            {
                SceneComponent->SetRelativeRotation(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")));
            }
            if (Params->HasField(TEXT("scale")))
            {
                SceneComponent->SetRelativeScale3D(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
            }
        }

        // Add to root if no parent specified
        Blueprint->SimpleConstructionScript->AddNode(NewNode);

        // Compile the blueprint
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("component_name"), ComponentName);
        ResultObj->SetStringField(TEXT("component_type"), ComponentType);
        return ResultObj;
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add component to blueprint"));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString FolderPath = TEXT("/Game/Materials");
    Params->TryGetStringField(TEXT("path"), FolderPath);
    FolderPath = NormalizeAssetPath(FolderPath, TEXT("/Game/Materials"));

    FString Preset = TEXT("standard");
    Params->TryGetStringField(TEXT("preset"), Preset);
    Preset = Preset.ToLower();

    const FString AssetPath = FolderPath / MaterialName;
    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material already exists: %s"), *AssetPath));
    }

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material package"));
    }

    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Cast<UMaterial>(
        Factory->FactoryCreateNew(
            UMaterial::StaticClass(),
            Package,
            *MaterialName,
            RF_Public | RF_Standalone,
            nullptr,
            GWarn
        )
    );

    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material"));
    }

    // Shared parameter set
    UMaterialExpressionVectorParameter* BaseColor = Cast<UMaterialExpressionVectorParameter>(AddExpr(Material, UMaterialExpressionVectorParameter::StaticClass(), -900, -200));
    UMaterialExpressionScalarParameter* Roughness = Cast<UMaterialExpressionScalarParameter>(AddExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -900, 50));
    UMaterialExpressionScalarParameter* Metallic = Cast<UMaterialExpressionScalarParameter>(AddExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -900, 220));
    UMaterialExpressionVectorParameter* EmissiveColor = Cast<UMaterialExpressionVectorParameter>(AddExpr(Material, UMaterialExpressionVectorParameter::StaticClass(), -900, 400));
    UMaterialExpressionScalarParameter* EmissiveStrength = Cast<UMaterialExpressionScalarParameter>(AddExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -900, 560));
    UMaterialExpressionMultiply* EmissiveMul = Cast<UMaterialExpressionMultiply>(AddExpr(Material, UMaterialExpressionMultiply::StaticClass(), -600, 480));
    UMaterialExpressionScalarParameter* Opacity = Cast<UMaterialExpressionScalarParameter>(AddExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -900, 760));

    BaseColor->ParameterName = TEXT("BaseColor");
    BaseColor->DefaultValue = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);
    Roughness->ParameterName = TEXT("Roughness");
    Roughness->DefaultValue = 0.5f;
    Metallic->ParameterName = TEXT("Metallic");
    Metallic->DefaultValue = 0.0f;
    EmissiveColor->ParameterName = TEXT("EmissiveColor");
    EmissiveColor->DefaultValue = FLinearColor::Black;
    EmissiveStrength->ParameterName = TEXT("EmissiveStrength");
    EmissiveStrength->DefaultValue = 0.0f;
    Opacity->ParameterName = TEXT("Opacity");
    Opacity->DefaultValue = 1.0f;

    UMaterialEditingLibrary::ConnectMaterialExpressions(EmissiveColor, TEXT(""), EmissiveMul, TEXT("A"));
    UMaterialEditingLibrary::ConnectMaterialExpressions(EmissiveStrength, TEXT(""), EmissiveMul, TEXT("B"));

    UMaterialEditingLibrary::ConnectMaterialProperty(Roughness, TEXT(""), MP_Roughness);
    UMaterialEditingLibrary::ConnectMaterialProperty(Metallic, TEXT(""), MP_Metallic);
    UMaterialEditingLibrary::ConnectMaterialProperty(EmissiveMul, TEXT(""), MP_EmissiveColor);
    UMaterialEditingLibrary::ConnectMaterialProperty(Opacity, TEXT(""), MP_Opacity);

    if (Preset == TEXT("toon"))
    {
        UMaterialExpressionVectorParameter* OutlineColor = Cast<UMaterialExpressionVectorParameter>(AddExpr(Material, UMaterialExpressionVectorParameter::StaticClass(), -900, -20));
        UMaterialExpressionScalarParameter* OutlineStrength = Cast<UMaterialExpressionScalarParameter>(AddExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -900, 120));
        UMaterialExpressionFresnel* Fresnel = Cast<UMaterialExpressionFresnel>(AddExpr(Material, UMaterialExpressionFresnel::StaticClass(), -650, 0));
        UMaterialExpressionMultiply* OutlineMul = Cast<UMaterialExpressionMultiply>(AddExpr(Material, UMaterialExpressionMultiply::StaticClass(), -450, 40));
        UMaterialExpressionLinearInterpolate* ToonLerp = Cast<UMaterialExpressionLinearInterpolate>(AddExpr(Material, UMaterialExpressionLinearInterpolate::StaticClass(), -250, -80));

        OutlineColor->ParameterName = TEXT("OutlineColor");
        OutlineColor->DefaultValue = FLinearColor::Black;
        OutlineStrength->ParameterName = TEXT("OutlineStrength");
        OutlineStrength->DefaultValue = 0.5f;
        Fresnel->Exponent = 4.0f;

        UMaterialEditingLibrary::ConnectMaterialExpressions(Fresnel, TEXT(""), OutlineMul, TEXT("A"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(OutlineStrength, TEXT(""), OutlineMul, TEXT("B"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(BaseColor, TEXT(""), ToonLerp, TEXT("A"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(OutlineColor, TEXT(""), ToonLerp, TEXT("B"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(OutlineMul, TEXT(""), ToonLerp, TEXT("Alpha"));
        UMaterialEditingLibrary::ConnectMaterialProperty(ToonLerp, TEXT(""), MP_BaseColor);
    }
    else if (Preset == TEXT("hatch"))
    {
        UMaterialExpressionVectorParameter* HatchColor = Cast<UMaterialExpressionVectorParameter>(AddExpr(Material, UMaterialExpressionVectorParameter::StaticClass(), -900, -20));
        UMaterialExpressionScalarParameter* HatchDensity = Cast<UMaterialExpressionScalarParameter>(AddExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -900, 120));
        UMaterialExpressionScalarParameter* HatchStrength = Cast<UMaterialExpressionScalarParameter>(AddExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -900, 260));
        UMaterialExpressionWorldPosition* WorldPos = Cast<UMaterialExpressionWorldPosition>(AddExpr(Material, UMaterialExpressionWorldPosition::StaticClass(), -700, -20));
        UMaterialExpressionComponentMask* MaskX = Cast<UMaterialExpressionComponentMask>(AddExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -520, -80));
        UMaterialExpressionComponentMask* MaskY = Cast<UMaterialExpressionComponentMask>(AddExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -520, 40));
        UMaterialExpressionMultiply* MulX = Cast<UMaterialExpressionMultiply>(AddExpr(Material, UMaterialExpressionMultiply::StaticClass(), -360, -100));
        UMaterialExpressionMultiply* MulY = Cast<UMaterialExpressionMultiply>(AddExpr(Material, UMaterialExpressionMultiply::StaticClass(), -360, 20));
        UMaterialExpressionSine* SinX = Cast<UMaterialExpressionSine>(AddExpr(Material, UMaterialExpressionSine::StaticClass(), -200, -100));
        UMaterialExpressionSine* SinY = Cast<UMaterialExpressionSine>(AddExpr(Material, UMaterialExpressionSine::StaticClass(), -200, 20));
        UMaterialExpressionAbs* AbsX = Cast<UMaterialExpressionAbs>(AddExpr(Material, UMaterialExpressionAbs::StaticClass(), -70, -100));
        UMaterialExpressionAbs* AbsY = Cast<UMaterialExpressionAbs>(AddExpr(Material, UMaterialExpressionAbs::StaticClass(), -70, 20));
        UMaterialExpressionMultiply* HatchPattern = Cast<UMaterialExpressionMultiply>(AddExpr(Material, UMaterialExpressionMultiply::StaticClass(), 70, -50));
        UMaterialExpressionSaturate* HatchSat = Cast<UMaterialExpressionSaturate>(AddExpr(Material, UMaterialExpressionSaturate::StaticClass(), 220, -50));
        UMaterialExpressionMultiply* HatchAlpha = Cast<UMaterialExpressionMultiply>(AddExpr(Material, UMaterialExpressionMultiply::StaticClass(), 390, -10));
        UMaterialExpressionLinearInterpolate* HatchLerp = Cast<UMaterialExpressionLinearInterpolate>(AddExpr(Material, UMaterialExpressionLinearInterpolate::StaticClass(), 560, -60));

        HatchColor->ParameterName = TEXT("HatchColor");
        HatchColor->DefaultValue = FLinearColor::Black;
        HatchDensity->ParameterName = TEXT("HatchDensity");
        HatchDensity->DefaultValue = 22.0f;
        HatchStrength->ParameterName = TEXT("HatchStrength");
        HatchStrength->DefaultValue = 0.6f;
        MaskX->R = true;
        MaskY->G = true;

        UMaterialEditingLibrary::ConnectMaterialExpressions(WorldPos, TEXT(""), MaskX, TEXT("Input"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(WorldPos, TEXT(""), MaskY, TEXT("Input"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(MaskX, TEXT(""), MulX, TEXT("A"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(MaskY, TEXT(""), MulY, TEXT("A"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(HatchDensity, TEXT(""), MulX, TEXT("B"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(HatchDensity, TEXT(""), MulY, TEXT("B"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(MulX, TEXT(""), SinX, TEXT("Input"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(MulY, TEXT(""), SinY, TEXT("Input"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(SinX, TEXT(""), AbsX, TEXT("Input"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(SinY, TEXT(""), AbsY, TEXT("Input"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(AbsX, TEXT(""), HatchPattern, TEXT("A"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(AbsY, TEXT(""), HatchPattern, TEXT("B"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(HatchPattern, TEXT(""), HatchSat, TEXT("Input"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(HatchSat, TEXT(""), HatchAlpha, TEXT("A"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(HatchStrength, TEXT(""), HatchAlpha, TEXT("B"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(BaseColor, TEXT(""), HatchLerp, TEXT("A"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(HatchColor, TEXT(""), HatchLerp, TEXT("B"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(HatchAlpha, TEXT(""), HatchLerp, TEXT("Alpha"));
        UMaterialEditingLibrary::ConnectMaterialProperty(HatchLerp, TEXT(""), MP_BaseColor);
    }
    else
    {
        UMaterialEditingLibrary::ConnectMaterialProperty(BaseColor, TEXT(""), MP_BaseColor);
    }

    if (Preset == TEXT("water") || Preset == TEXT("ice"))
    {
        Material->BlendMode = BLEND_Translucent;
        Material->TwoSided = false;
    }
    else
    {
        Material->BlendMode = BLEND_Opaque;
    }

    UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Material);
    UEditorAssetLibrary::SaveLoadedAsset(Material, false);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), MaterialName);
    ResultObj->SetStringField(TEXT("path"), AssetPath);
    ResultObj->SetStringField(TEXT("preset"), Preset);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString InstanceName;
    if (!Params->TryGetStringField(TEXT("name"), InstanceName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString ParentMaterialPath;
    if (!Params->TryGetStringField(TEXT("parent_material"), ParentMaterialPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_material' parameter"));
    }

    FString InstanceFolder = TEXT("/Game/Materials/Instances");
    Params->TryGetStringField(TEXT("path"), InstanceFolder);
    InstanceFolder = NormalizeAssetPath(InstanceFolder, TEXT("/Game/Materials/Instances"));

    if (ParentMaterialPath.Contains(TEXT(".")))
    {
        ParentMaterialPath = ParentMaterialPath.Left(ParentMaterialPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
    }
    ParentMaterialPath = NormalizeAssetPath(ParentMaterialPath, TEXT("/Game/Materials"));

    UMaterialInterface* ParentMaterial = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(ParentMaterialPath));
    if (!ParentMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent material not found: %s"), *ParentMaterialPath));
    }

    const FString AssetPath = InstanceFolder / InstanceName;
    if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance already exists: %s"), *AssetPath));
    }

    UPackage* Package = CreatePackage(*AssetPath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material instance package"));
    }

    UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = ParentMaterial;
    UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(
        Factory->FactoryCreateNew(
            UMaterialInstanceConstant::StaticClass(),
            Package,
            *InstanceName,
            RF_Public | RF_Standalone,
            nullptr,
            GWarn
        )
    );

    if (!MIC)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material instance"));
    }

    MIC->SetParentEditorOnly(ParentMaterial);
    MIC->PostEditChange();
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(MIC);
    UEditorAssetLibrary::SaveLoadedAsset(MIC, false);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), InstanceName);
    ResultObj->SetStringField(TEXT("path"), AssetPath);
    ResultObj->SetStringField(TEXT("parent_material"), ParentMaterialPath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetMaterialParam(const TSharedPtr<FJsonObject>& Params)
{
    FString InstancePath;
    if (!Params->TryGetStringField(TEXT("material_instance"), InstancePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_instance' parameter"));
    }

    FString ParamType;
    if (!Params->TryGetStringField(TEXT("param_type"), ParamType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'param_type' parameter"));
    }

    FString ParamName;
    if (!Params->TryGetStringField(TEXT("param_name"), ParamName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'param_name' parameter"));
    }

    if (!Params->HasField(TEXT("value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
    }

    if (InstancePath.Contains(TEXT(".")))
    {
        InstancePath = InstancePath.Left(InstancePath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
    }
    InstancePath = NormalizeAssetPath(InstancePath, TEXT("/Game/Materials/Instances"));

    UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(UEditorAssetLibrary::LoadAsset(InstancePath));
    if (!MIC)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material instance not found: %s"), *InstancePath));
    }

    const FString Type = ParamType.ToLower();
    const TSharedPtr<FJsonValue> Value = Params->Values.FindRef(TEXT("value"));
    bool bSuccess = false;
    FString ErrorMessage;

    FMaterialParameterInfo ParameterInfo(*ParamName);
    TArray<FMaterialParameterInfo> ScalarInfos;
    TArray<FMaterialParameterInfo> VectorInfos;
    TArray<FMaterialParameterInfo> TextureInfos;
    TArray<FGuid> Ids;
    MIC->GetAllScalarParameterInfo(ScalarInfos, Ids);
    MIC->GetAllVectorParameterInfo(VectorInfos, Ids);
    MIC->GetAllTextureParameterInfo(TextureInfos, Ids);

    auto ContainsParam = [&ParamName](const TArray<FMaterialParameterInfo>& Infos) -> bool
    {
        for (const FMaterialParameterInfo& Info : Infos)
        {
            if (Info.Name.ToString() == ParamName)
            {
                return true;
            }
        }
        return false;
    };

    if (Type == TEXT("scalar"))
    {
        if (ContainsParam(ScalarInfos))
        {
            MIC->SetScalarParameterValueEditorOnly(ParameterInfo, static_cast<float>(Value->AsNumber()));
            bSuccess = true;
        }
    }
    else if (Type == TEXT("vector"))
    {
        FLinearColor Color = FLinearColor::White;
        if (Value->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& Array = Value->AsArray();
            if (Array.Num() >= 3)
            {
                Color.R = static_cast<float>(Array[0]->AsNumber());
                Color.G = static_cast<float>(Array[1]->AsNumber());
                Color.B = static_cast<float>(Array[2]->AsNumber());
                Color.A = Array.Num() >= 4 ? static_cast<float>(Array[3]->AsNumber()) : 1.0f;
                if (ContainsParam(VectorInfos))
                {
                    MIC->SetVectorParameterValueEditorOnly(ParameterInfo, Color);
                    bSuccess = true;
                }
            }
            else
            {
                ErrorMessage = TEXT("Vector value must have at least 3 elements");
            }
        }
        else
        {
            ErrorMessage = TEXT("Vector value must be an array [r,g,b,a]");
        }
    }
    else if (Type == TEXT("texture"))
    {
        FString TexturePath = Value->AsString();
        if (TexturePath.Contains(TEXT(".")))
        {
            TexturePath = TexturePath.Left(TexturePath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
        }
        TexturePath = NormalizeAssetPath(TexturePath, TEXT("/Game"));

        UTexture* Texture = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexturePath));
        if (!Texture)
        {
            ErrorMessage = FString::Printf(TEXT("Texture not found: %s"), *TexturePath);
        }
        else
        {
            if (ContainsParam(TextureInfos))
            {
                MIC->SetTextureParameterValueEditorOnly(ParameterInfo, Texture);
                bSuccess = true;
            }
        }
    }
    else
    {
        ErrorMessage = FString::Printf(TEXT("Unsupported param_type: %s"), *ParamType);
    }

    if (!bSuccess)
    {
        if (ErrorMessage.IsEmpty())
        {
            auto JoinInfoNames = [](const TArray<FMaterialParameterInfo>& Infos) -> FString
            {
                FString Out;
                for (int32 i = 0; i < Infos.Num(); ++i)
                {
                    if (i > 0)
                    {
                        Out += TEXT(", ");
                    }
                    Out += Infos[i].Name.ToString();
                }
                return Out;
            };

            ErrorMessage = FString::Printf(
                TEXT("Failed to set parameter '%s' (type=%s). Available scalar=[%s], vector=[%s], texture=[%s]"),
                *ParamName,
                *Type,
                *JoinInfoNames(ScalarInfos),
                *JoinInfoNames(VectorInfos),
                *JoinInfoNames(TextureInfos)
            );
        }
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }

    MIC->PostEditChange();
    MIC->MarkPackageDirty();
    UEditorAssetLibrary::SaveLoadedAsset(MIC, false);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_instance"), InstancePath);
    ResultObj->SetStringField(TEXT("param_type"), Type);
    ResultObj->SetStringField(TEXT("param_name"), ParamName);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleAssignMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString TargetName;
    if (!Params->TryGetStringField(TEXT("target"), TargetName) && !Params->TryGetStringField(TEXT("actor_name"), TargetName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target' or 'actor_name' parameter"));
    }

    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material"), MaterialPath) && !Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material' or 'material_path' parameter"));
    }

    if (MaterialPath.Contains(TEXT(".")))
    {
        MaterialPath = MaterialPath.Left(MaterialPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
    }
    MaterialPath = NormalizeAssetPath(MaterialPath, TEXT("/Game/Materials"));

    int32 SlotIndex = 0;
    if (Params->HasField(TEXT("slot_index")))
    {
        SlotIndex = Params->GetIntegerField(TEXT("slot_index"));
    }

    UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

    AActor* TargetActor = nullptr;
    for (AActor* Actor : AllActors)
    {
        if (!Actor)
        {
            continue;
        }

        if (Actor->GetName() == TargetName || Actor->GetActorLabel() == TargetName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *TargetName));
    }

    TArray<UMeshComponent*> MeshComponents;
    TargetActor->GetComponents<UMeshComponent>(MeshComponents);

    if (MeshComponents.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor has no mesh components: %s"), *TargetName));
    }

    int32 AppliedCount = 0;
    for (UMeshComponent* MeshComp : MeshComponents)
    {
        if (!MeshComp)
        {
            continue;
        }
        MeshComp->SetMaterial(SlotIndex, Material);
        MeshComp->MarkRenderStateDirty();
        AppliedCount++;
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("target"), TargetName);
    ResultObj->SetStringField(TEXT("material"), MaterialPath);
    ResultObj->SetNumberField(TEXT("slot_index"), SlotIndex);
    ResultObj->SetNumberField(TEXT("components_updated"), AppliedCount);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Log all input parameters for debugging
    UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - Blueprint: %s, Component: %s, Property: %s"), 
        *BlueprintName, *ComponentName, *PropertyName);
    
    // Log property_value if available
    if (Params->HasField(TEXT("property_value")))
    {
        TSharedPtr<FJsonValue> JsonValue = Params->Values.FindRef(TEXT("property_value"));
        FString ValueType;
        
        switch(JsonValue->Type)
        {
            case EJson::Boolean: ValueType = FString::Printf(TEXT("Boolean: %s"), JsonValue->AsBool() ? TEXT("true") : TEXT("false")); break;
            case EJson::Number: ValueType = FString::Printf(TEXT("Number: %f"), JsonValue->AsNumber()); break;
            case EJson::String: ValueType = FString::Printf(TEXT("String: %s"), *JsonValue->AsString()); break;
            case EJson::Array: ValueType = TEXT("Array"); break;
            case EJson::Object: ValueType = TEXT("Object"); break;
            default: ValueType = TEXT("Unknown"); break;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - Value Type: %s"), *ValueType);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - No property_value provided"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Blueprint not found: %s"), *BlueprintName);
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Blueprint found: %s (Class: %s)"), 
            *BlueprintName, 
            Blueprint->GeneratedClass ? *Blueprint->GeneratedClass->GetName() : TEXT("NULL"));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Searching for component %s in blueprint nodes"), *ComponentName);
    
    if (!Blueprint->SimpleConstructionScript)
    {
        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - SimpleConstructionScript is NULL for blueprint %s"), *BlueprintName);
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint construction script"));
    }
    
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node)
        {
            UE_LOG(LogTemp, Verbose, TEXT("SetComponentProperty - Found node: %s"), *Node->GetVariableName().ToString());
            if (Node->GetVariableName().ToString() == ComponentName)
            {
                ComponentNode = Node;
                break;
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - Found NULL node in blueprint"));
        }
    }

    if (!ComponentNode)
    {
        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Component not found: %s"), *ComponentName);
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Component found: %s (Class: %s)"), 
            *ComponentName, 
            ComponentNode->ComponentTemplate ? *ComponentNode->ComponentTemplate->GetClass()->GetName() : TEXT("NULL"));
    }

    // Get the component template
    UObject* ComponentTemplate = ComponentNode->ComponentTemplate;
    if (!ComponentTemplate)
    {
        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Component template is NULL for %s"), *ComponentName);
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Invalid component template"));
    }

    // Check if this is a Spring Arm component and log special debug info
    if (ComponentTemplate->GetClass()->GetName().Contains(TEXT("SpringArm")))
    {
        UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - SpringArm component detected! Class: %s"), 
            *ComponentTemplate->GetClass()->GetPathName());
            
        // Log all properties of the SpringArm component class
        UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - SpringArm properties:"));
        for (TFieldIterator<FProperty> PropIt(ComponentTemplate->GetClass()); PropIt; ++PropIt)
        {
            FProperty* Prop = *PropIt;
            UE_LOG(LogTemp, Warning, TEXT("  - %s (%s)"), *Prop->GetName(), *Prop->GetCPPType());
        }

        // Special handling for Spring Arm properties
        if (Params->HasField(TEXT("property_value")))
        {
            TSharedPtr<FJsonValue> JsonValue = Params->Values.FindRef(TEXT("property_value"));
            
            // Get the property using the new FField system
            FProperty* Property = FindFProperty<FProperty>(ComponentTemplate->GetClass(), *PropertyName);
            if (!Property)
            {
                UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Property %s not found on SpringArm component"), *PropertyName);
                return FUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Property %s not found on SpringArm component"), *PropertyName));
            }

            // Create a scope guard to ensure property cleanup
            struct FScopeGuard
            {
                UObject* Object;
                FScopeGuard(UObject* InObject) : Object(InObject) 
                {
                    if (Object)
                    {
                        Object->Modify();
                    }
                }
                ~FScopeGuard()
                {
                    if (Object)
                    {
                        Object->PostEditChange();
                    }
                }
            } ScopeGuard(ComponentTemplate);

            bool bSuccess = false;
            FString ErrorMessage;

            // Handle specific Spring Arm property types
            if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
            {
                if (JsonValue->Type == EJson::Number)
                {
                    const float Value = JsonValue->AsNumber();
                    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting float property %s to %f"), *PropertyName, Value);
                    FloatProp->SetPropertyValue_InContainer(ComponentTemplate, Value);
                    bSuccess = true;
                }
            }
            else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
            {
                if (JsonValue->Type == EJson::Boolean)
                {
                    const bool Value = JsonValue->AsBool();
                    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting bool property %s to %d"), *PropertyName, Value);
                    BoolProp->SetPropertyValue_InContainer(ComponentTemplate, Value);
                    bSuccess = true;
                }
            }
            else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
            {
                UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Handling struct property %s of type %s"), 
                    *PropertyName, *StructProp->Struct->GetName());
                
                // Special handling for common Spring Arm struct properties
                if (StructProp->Struct == TBaseStructure<FVector>::Get())
                {
                    if (JsonValue->Type == EJson::Array)
                    {
                        const TArray<TSharedPtr<FJsonValue>>& Arr = JsonValue->AsArray();
                        if (Arr.Num() == 3)
                        {
                            FVector Vec(
                                Arr[0]->AsNumber(),
                                Arr[1]->AsNumber(),
                                Arr[2]->AsNumber()
                            );
                            void* PropertyAddr = StructProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
                            StructProp->CopySingleValue(PropertyAddr, &Vec);
                            bSuccess = true;
                        }
                    }
                }
                else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
                {
                    if (JsonValue->Type == EJson::Array)
                    {
                        const TArray<TSharedPtr<FJsonValue>>& Arr = JsonValue->AsArray();
                        if (Arr.Num() == 3)
                        {
                            FRotator Rot(
                                Arr[0]->AsNumber(),
                                Arr[1]->AsNumber(),
                                Arr[2]->AsNumber()
                            );
                            void* PropertyAddr = StructProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
                            StructProp->CopySingleValue(PropertyAddr, &Rot);
                            bSuccess = true;
                        }
                    }
                }
            }

            if (bSuccess)
            {
                // Mark the blueprint as modified
                UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Successfully set SpringArm property %s"), *PropertyName);
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("component"), ComponentName);
                ResultObj->SetStringField(TEXT("property"), PropertyName);
                ResultObj->SetBoolField(TEXT("success"), true);
                return ResultObj;
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Failed to set SpringArm property %s"), *PropertyName);
                return FUnrealMCPCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Failed to set SpringArm property %s"), *PropertyName));
            }
        }
    }

    // Regular property handling for non-Spring Arm components continues...

    // Set the property value
    if (Params->HasField(TEXT("property_value")))
    {
        TSharedPtr<FJsonValue> JsonValue = Params->Values.FindRef(TEXT("property_value"));
        
        // Get the property
        FProperty* Property = FindFProperty<FProperty>(ComponentTemplate->GetClass(), *PropertyName);
        if (!Property)
        {
            UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Property %s not found on component %s"), 
                *PropertyName, *ComponentName);
            
            // List all available properties for this component
            UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - Available properties for %s:"), *ComponentName);
            for (TFieldIterator<FProperty> PropIt(ComponentTemplate->GetClass()); PropIt; ++PropIt)
            {
                FProperty* Prop = *PropIt;
                UE_LOG(LogTemp, Warning, TEXT("  - %s (%s)"), *Prop->GetName(), *Prop->GetCPPType());
            }
            
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Property %s not found on component %s"), *PropertyName, *ComponentName));
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Property found: %s (Type: %s)"), 
                *PropertyName, *Property->GetCPPType());
        }

        bool bSuccess = false;
        FString ErrorMessage;

        // Handle different property types
        UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Attempting to set property %s"), *PropertyName);
        
        // Add try-catch block to catch and log any crashes
        try
        {
            if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
            {
                // Handle vector properties
                UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Property is a struct: %s"), 
                    StructProp->Struct ? *StructProp->Struct->GetName() : TEXT("NULL"));
                    
                if (StructProp->Struct == TBaseStructure<FVector>::Get())
                {
                    if (JsonValue->Type == EJson::Array)
                    {
                        // Handle array input [x, y, z]
                        const TArray<TSharedPtr<FJsonValue>>& Arr = JsonValue->AsArray();
                        if (Arr.Num() == 3)
                        {
                            FVector Vec(
                                Arr[0]->AsNumber(),
                                Arr[1]->AsNumber(),
                                Arr[2]->AsNumber()
                            );
                            void* PropertyAddr = StructProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
                            UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting Vector(%f, %f, %f)"), 
                                Vec.X, Vec.Y, Vec.Z);
                            StructProp->CopySingleValue(PropertyAddr, &Vec);
                            bSuccess = true;
                        }
                        else
                        {
                            ErrorMessage = FString::Printf(TEXT("Vector property requires 3 values, got %d"), Arr.Num());
                            UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - %s"), *ErrorMessage);
                        }
                    }
                    else if (JsonValue->Type == EJson::Number)
                    {
                        // Handle scalar input (sets all components to same value)
                        float Value = JsonValue->AsNumber();
                        FVector Vec(Value, Value, Value);
                        void* PropertyAddr = StructProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
                        UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting Vector(%f, %f, %f) from scalar"), 
                            Vec.X, Vec.Y, Vec.Z);
                        StructProp->CopySingleValue(PropertyAddr, &Vec);
                        bSuccess = true;
                    }
                    else
                    {
                        ErrorMessage = TEXT("Vector property requires either a single number or array of 3 numbers");
                        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - %s"), *ErrorMessage);
                    }
                }
                else
                {
                    // Handle other struct properties using default handler
                    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Using generic struct handler for %s"), 
                        *PropertyName);
                    bSuccess = FUnrealMCPCommonUtils::SetObjectProperty(ComponentTemplate, PropertyName, JsonValue, ErrorMessage);
                    if (!bSuccess)
                    {
                        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Failed to set struct property: %s"), *ErrorMessage);
                    }
                }
            }
            else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
            {
                // Handle enum properties
                UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Property is an enum"));
                if (JsonValue->Type == EJson::String)
                {
                    FString EnumValueName = JsonValue->AsString();
                    UEnum* Enum = EnumProp->GetEnum();
                    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting enum from string: %s"), *EnumValueName);
                    
                    if (Enum)
                    {
                        int64 EnumValue = Enum->GetValueByNameString(EnumValueName);
                        
                        if (EnumValue != INDEX_NONE)
                        {
                            UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Found enum value: %lld"), EnumValue);
                            EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(
                                ComponentTemplate, 
                                EnumValue
                            );
                            bSuccess = true;
                        }
                        else
                        {
                            // List all possible enum values
                            UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - Available enum values for %s:"), 
                                *Enum->GetName());
                            for (int32 i = 0; i < Enum->NumEnums(); i++)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("  - %s (%lld)"), 
                                    *Enum->GetNameStringByIndex(i),
                                    Enum->GetValueByIndex(i));
                            }
                            
                            ErrorMessage = FString::Printf(TEXT("Invalid enum value '%s' for property %s"), 
                                *EnumValueName, *PropertyName);
                            UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - %s"), *ErrorMessage);
                        }
                    }
                    else
                    {
                        ErrorMessage = TEXT("Enum object is NULL");
                        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - %s"), *ErrorMessage);
                    }
                }
                else if (JsonValue->Type == EJson::Number)
                {
                    // Allow setting enum by integer value
                    int64 EnumValue = JsonValue->AsNumber();
                    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting enum from number: %lld"), EnumValue);
                    EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(
                        ComponentTemplate, 
                        EnumValue
                    );
                    bSuccess = true;
                }
                else
                {
                    ErrorMessage = TEXT("Enum property requires either a string name or integer value");
                    UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - %s"), *ErrorMessage);
                }
            }
            else if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
            {
                // Handle numeric properties
                UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Property is numeric: IsInteger=%d, IsFloat=%d"), 
                    NumericProp->IsInteger(), NumericProp->IsFloatingPoint());
                    
                if (JsonValue->Type == EJson::Number)
                {
                    double Value = JsonValue->AsNumber();
                    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting numeric value: %f"), Value);
                    
                    if (NumericProp->IsInteger())
                    {
                        NumericProp->SetIntPropertyValue(ComponentTemplate, (int64)Value);
                        UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Set integer value: %lld"), (int64)Value);
                        bSuccess = true;
                    }
                    else if (NumericProp->IsFloatingPoint())
                    {
                        NumericProp->SetFloatingPointPropertyValue(ComponentTemplate, Value);
                        UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Set float value: %f"), Value);
                        bSuccess = true;
                    }
                }
                else
                {
                    ErrorMessage = TEXT("Numeric property requires a number value");
                    UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - %s"), *ErrorMessage);
                }
            }
            else
            {
                // Handle all other property types using default handler
                UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Using generic property handler for %s (Type: %s)"), 
                    *PropertyName, *Property->GetCPPType());
                bSuccess = FUnrealMCPCommonUtils::SetObjectProperty(ComponentTemplate, PropertyName, JsonValue, ErrorMessage);
                if (!bSuccess)
                {
                    UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Failed to set property: %s"), *ErrorMessage);
                }
            }
        }
        catch (const std::exception& Ex)
        {
            UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - EXCEPTION: %s"), ANSI_TO_TCHAR(Ex.what()));
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Exception while setting property %s: %s"), *PropertyName, ANSI_TO_TCHAR(Ex.what())));
        }
        catch (...)
        {
            UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - UNKNOWN EXCEPTION occurred while setting property %s"), *PropertyName);
            return FUnrealMCPCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Unknown exception while setting property %s"), *PropertyName));
        }

        if (bSuccess)
        {
            // Mark the blueprint as modified
            UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Successfully set property %s on component %s"), 
                *PropertyName, *ComponentName);
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetStringField(TEXT("component"), ComponentName);
            ResultObj->SetStringField(TEXT("property"), PropertyName);
            ResultObj->SetBoolField(TEXT("success"), true);
            return ResultObj;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Failed to set property %s: %s"), 
                *PropertyName, *ErrorMessage);
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }
    }

    UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Missing 'property_value' parameter"));
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    // Set physics properties
    if (Params->HasField(TEXT("simulate_physics")))
    {
        PrimComponent->SetSimulatePhysics(Params->GetBoolField(TEXT("simulate_physics")));
    }

    if (Params->HasField(TEXT("mass")))
    {
        float Mass = Params->GetNumberField(TEXT("mass"));
        // In UE5.5, use proper overrideMass instead of just scaling
        PrimComponent->SetMassOverrideInKg(NAME_None, Mass);
        UE_LOG(LogTemp, Display, TEXT("Set mass for component %s to %f kg"), *ComponentName, Mass);
    }

    if (Params->HasField(TEXT("linear_damping")))
    {
        PrimComponent->SetLinearDamping(Params->GetNumberField(TEXT("linear_damping")));
    }

    if (Params->HasField(TEXT("angular_damping")))
    {
        PrimComponent->SetAngularDamping(Params->GetNumberField(TEXT("angular_damping")));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Compile the blueprint
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), BlueprintName);
    ResultObj->SetBoolField(TEXT("compiled"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform);
    if (NewActor)
    {
        NewActor->SetActorLabel(*ActorName);
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetBlueprintProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the default object
    UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
    if (!DefaultObject)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get default object"));
    }

    // Set the property value
    if (Params->HasField(TEXT("property_value")))
    {
        TSharedPtr<FJsonValue> JsonValue = Params->Values.FindRef(TEXT("property_value"));
        
        FString ErrorMessage;
        if (FUnrealMCPCommonUtils::SetObjectProperty(DefaultObject, PropertyName, JsonValue, ErrorMessage))
        {
            // Mark the blueprint as modified
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetStringField(TEXT("property"), PropertyName);
            ResultObj->SetBoolField(TEXT("success"), true);
            return ResultObj;
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
        }
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
    if (!MeshComponent)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Component is not a static mesh component"));
    }

    // Set static mesh properties
    if (Params->HasField(TEXT("static_mesh")))
    {
        FString MeshPath = Params->GetStringField(TEXT("static_mesh"));
        UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
        if (Mesh)
        {
            MeshComponent->SetStaticMesh(Mesh);
        }
    }

    if (Params->HasField(TEXT("material")))
    {
        FString MaterialPath = Params->GetStringField(TEXT("material"));
        UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (Material)
        {
            MeshComponent->SetMaterial(0, Material);
        }
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintCommands::HandleSetPawnProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the default object
    UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
    if (!DefaultObject)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get default object"));
    }

    // Track if any properties were set successfully
    bool bAnyPropertiesSet = false;
    TSharedPtr<FJsonObject> ResultsObj = MakeShared<FJsonObject>();
    
    // Set auto possess player if specified
    if (Params->HasField(TEXT("auto_possess_player")))
    {
        TSharedPtr<FJsonValue> AutoPossessValue = Params->Values.FindRef(TEXT("auto_possess_player"));
        
        FString ErrorMessage;
        if (FUnrealMCPCommonUtils::SetObjectProperty(DefaultObject, TEXT("AutoPossessPlayer"), AutoPossessValue, ErrorMessage))
        {
            bAnyPropertiesSet = true;
            TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
            PropResultObj->SetBoolField(TEXT("success"), true);
            ResultsObj->SetObjectField(TEXT("AutoPossessPlayer"), PropResultObj);
        }
        else
        {
            TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
            PropResultObj->SetBoolField(TEXT("success"), false);
            PropResultObj->SetStringField(TEXT("error"), ErrorMessage);
            ResultsObj->SetObjectField(TEXT("AutoPossessPlayer"), PropResultObj);
        }
    }
    
    // Set controller rotation properties
    const TCHAR* RotationProps[] = {
        TEXT("bUseControllerRotationYaw"),
        TEXT("bUseControllerRotationPitch"),
        TEXT("bUseControllerRotationRoll")
    };
    
    const TCHAR* ParamNames[] = {
        TEXT("use_controller_rotation_yaw"),
        TEXT("use_controller_rotation_pitch"),
        TEXT("use_controller_rotation_roll")
    };
    
    for (int32 i = 0; i < 3; i++)
    {
        if (Params->HasField(ParamNames[i]))
        {
            TSharedPtr<FJsonValue> Value = Params->Values.FindRef(ParamNames[i]);
            
            FString ErrorMessage;
            if (FUnrealMCPCommonUtils::SetObjectProperty(DefaultObject, RotationProps[i], Value, ErrorMessage))
            {
                bAnyPropertiesSet = true;
                TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
                PropResultObj->SetBoolField(TEXT("success"), true);
                ResultsObj->SetObjectField(RotationProps[i], PropResultObj);
            }
            else
            {
                TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
                PropResultObj->SetBoolField(TEXT("success"), false);
                PropResultObj->SetStringField(TEXT("error"), ErrorMessage);
                ResultsObj->SetObjectField(RotationProps[i], PropResultObj);
            }
        }
    }
    
    // Set can be damaged property
    if (Params->HasField(TEXT("can_be_damaged")))
    {
        TSharedPtr<FJsonValue> Value = Params->Values.FindRef(TEXT("can_be_damaged"));
        
        FString ErrorMessage;
        if (FUnrealMCPCommonUtils::SetObjectProperty(DefaultObject, TEXT("bCanBeDamaged"), Value, ErrorMessage))
        {
            bAnyPropertiesSet = true;
            TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
            PropResultObj->SetBoolField(TEXT("success"), true);
            ResultsObj->SetObjectField(TEXT("bCanBeDamaged"), PropResultObj);
        }
        else
        {
            TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
            PropResultObj->SetBoolField(TEXT("success"), false);
            PropResultObj->SetStringField(TEXT("error"), ErrorMessage);
            ResultsObj->SetObjectField(TEXT("bCanBeDamaged"), PropResultObj);
        }
    }

    // Mark the blueprint as modified if any properties were set
    if (bAnyPropertiesSet)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }
    else if (ResultsObj->Values.Num() == 0)
    {
        // No properties were specified
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No properties specified to set"));
    }

    TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
    ResponseObj->SetStringField(TEXT("blueprint"), BlueprintName);
    ResponseObj->SetBoolField(TEXT("success"), bAnyPropertiesSet);
    ResponseObj->SetObjectField(TEXT("results"), ResultsObj);
    return ResponseObj;
} 
