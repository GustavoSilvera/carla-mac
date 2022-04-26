#include "DReyeVRPawn.h"

ADReyeVRPawn::ADReyeVRPawn(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    auto *RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DReyeVR_RootComponent"));
    SetRootComponent(RootComponent);

    // auto possess player 0
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    ReadConfigVariables();

    // Create a camera and attach to root component
    FirstPersonCam = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCam"));
    FirstPersonCam->PostProcessSettings = PostProcessingInit();
    FirstPersonCam->bUsePawnControlRotation = false; // free for VR movement
    FirstPersonCam->bLockToHmd = true;               // lock orientation and position to HMD
    FirstPersonCam->FieldOfView = FieldOfView;       // editable
    FirstPersonCam->SetupAttachment(RootComponent);
}

void ADReyeVRPawn::ReadConfigVariables()
{
    ReadConfigValue("EgoVehicle", "FieldOfView", FieldOfView);
    // wheel hardware
    ReadConfigValue("Hardware", "DeviceIdx", WheelDeviceIdx);
    ReadConfigValue("Hardware", "LogUpdates", bLogLogitechWheel);
}

void ADReyeVRPawn::BeginPlay()
{
    Super::BeginPlay();

    World = GetWorld();
    ensure(World != nullptr);

    // Initialize logitech steering wheel
    InitLogiWheel();
}

void ADReyeVRPawn::BeginDestroy()
{
    Super::BeginDestroy();

    if (bIsLogiConnected)
        DestroyLogiWheel(false);
}

void ADReyeVRPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (EgoVehicle != nullptr)
    {
        // Tick the logitech wheel
        TickLogiWheel();
    }
}

FPostProcessSettings ADReyeVRPawn::PostProcessingInit()
{
    // modifying from here: https://docs.unrealengine.com/4.27/en-US/API/Runtime/Engine/Engine/FPostProcessSettings/
    FPostProcessSettings PP;
    PP.bOverride_VignetteIntensity = true;
    PP.VignetteIntensity = 0.f;

    PP.ScreenPercentage = 100.f;

    PP.bOverride_BloomIntensity = true;
    PP.BloomIntensity = 0.f;

    PP.bOverride_SceneFringeIntensity = true;
    PP.SceneFringeIntensity = 0.f;

    PP.bOverride_LensFlareIntensity = true;
    PP.LensFlareIntensity = 0.f;

    PP.bOverride_GrainIntensity = true;
    PP.GrainIntensity = 0.f;
    // PP.MotionBlurAmount = 0.f;
    return PP;
}

void ADReyeVRPawn::SetupPlayerInputComponent(UInputComponent *PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    check(PlayerInputComponent);

    /// NOTE: an Action is a digital input, an Axis is an analog input
    // steering and throttle analog inputs (axes)
    PlayerInputComponent->BindAxis("Steer_DReyeVR", this, &ADReyeVRPawn::SetSteeringKbd);
    PlayerInputComponent->BindAxis("Throttle_DReyeVR", this, &ADReyeVRPawn::SetThrottleKbd);
    PlayerInputComponent->BindAxis("Brake_DReyeVR", this, &ADReyeVRPawn::SetBrakeKbd);
    // // button actions (press & release)
    PlayerInputComponent->BindAction("ToggleReverse_DReyeVR", IE_Pressed, this, &ADReyeVRPawn::PressReverse);
    PlayerInputComponent->BindAction("TurnSignalRight_DReyeVR", IE_Pressed, this, &ADReyeVRPawn::PressTurnSignalR);
    PlayerInputComponent->BindAction("TurnSignalLeft_DReyeVR", IE_Pressed, this, &ADReyeVRPawn::PressTurnSignalL);
    PlayerInputComponent->BindAction("ToggleReverse_DReyeVR", IE_Released, this, &ADReyeVRPawn::ReleaseReverse);
    PlayerInputComponent->BindAction("TurnSignalRight_DReyeVR", IE_Released, this, &ADReyeVRPawn::ReleaseTurnSignalR);
    PlayerInputComponent->BindAction("TurnSignalLeft_DReyeVR", IE_Released, this, &ADReyeVRPawn::ReleaseTurnSignalL);
    PlayerInputComponent->BindAction("HoldHandbrake_DReyeVR", IE_Pressed, this, &ADReyeVRPawn::PressHandbrake);
    PlayerInputComponent->BindAction("HoldHandbrake_DReyeVR", IE_Released, this, &ADReyeVRPawn::ReleaseHandbrake);
    // clean/empty room experiment
    // PlayerInputComponent->BindAction("ToggleCleanRoom_DReyeVR", IE_Pressed, this, &ADReyeVRPawn::ToggleCleanRoom);
    /// Mouse X and Y input for looking up and turning
    PlayerInputComponent->BindAxis("MouseLookUp_DReyeVR", this, &ADReyeVRPawn::MouseLookUp);
    PlayerInputComponent->BindAxis("MouseTurn_DReyeVR", this, &ADReyeVRPawn::MouseTurn);
    // Camera position adjustments
    PlayerInputComponent->BindAction("CameraFwd_DReyeVR", IE_Pressed, this, &ADReyeVRPawn::CameraFwd);
    PlayerInputComponent->BindAction("CameraBack_DReyeVR", IE_Pressed, this, &ADReyeVRPawn::CameraBack);
    PlayerInputComponent->BindAction("CameraLeft_DReyeVR", IE_Pressed, this, &ADReyeVRPawn::CameraLeft);
    PlayerInputComponent->BindAction("CameraRight_DReyeVR", IE_Pressed, this, &ADReyeVRPawn::CameraRight);
    PlayerInputComponent->BindAction("CameraUp_DReyeVR", IE_Pressed, this, &ADReyeVRPawn::CameraUp);
    PlayerInputComponent->BindAction("CameraDown_DReyeVR", IE_Pressed, this, &ADReyeVRPawn::CameraDown);
}
/// TODO: REFACTOR THIS
#define CHECK_EGO_VEHICLE(FUNCTION)                                                                                    \
    if (EgoVehicle)                                                                                                    \
        FUNCTION;                                                                                                      \
    else                                                                                                               \
        UE_LOG(LogTemp, Warning, TEXT("No egovehicle!"));

void ADReyeVRPawn::SetThrottleKbd(const float ThrottleInput)
{
    if (ThrottleInput != 0)
    {
        CHECK_EGO_VEHICLE(EgoVehicle->SetThrottleKbd(ThrottleInput))
    }
}

void ADReyeVRPawn::SetBrakeKbd(const float BrakeInput)
{
    if (BrakeInput != 0)
    {
        CHECK_EGO_VEHICLE(EgoVehicle->SetBrakeKbd(BrakeInput))
    }
}

void ADReyeVRPawn::SetSteeringKbd(const float SteeringInput)
{
    if (SteeringInput != 0)
    {
        CHECK_EGO_VEHICLE(EgoVehicle->SetSteeringKbd(SteeringInput))
    }
    else
    {
        // so the steering wheel does go to 0 when letting go
        ensure(EgoVehicle != nullptr);
        EgoVehicle->VehicleInputs.Steering = 0;
    }
}

void ADReyeVRPawn::MouseLookUp(const float mY_Input)
{
    CHECK_EGO_VEHICLE(EgoVehicle->MouseLookUp(mY_Input))
}

void ADReyeVRPawn::MouseTurn(const float mX_Input)
{
    CHECK_EGO_VEHICLE(EgoVehicle->MouseTurn(mX_Input))
}

void ADReyeVRPawn::PressReverse()
{
    CHECK_EGO_VEHICLE(EgoVehicle->PressReverse())
}

void ADReyeVRPawn::ReleaseReverse()
{
    CHECK_EGO_VEHICLE(EgoVehicle->ReleaseReverse())
}

void ADReyeVRPawn::PressTurnSignalL()
{
    CHECK_EGO_VEHICLE(EgoVehicle->PressTurnSignalL())
}

void ADReyeVRPawn::ReleaseTurnSignalL()
{
    CHECK_EGO_VEHICLE(EgoVehicle->ReleaseTurnSignalL())
}

void ADReyeVRPawn::PressTurnSignalR()
{
    CHECK_EGO_VEHICLE(EgoVehicle->PressTurnSignalR())
}

void ADReyeVRPawn::ReleaseTurnSignalR()
{
    CHECK_EGO_VEHICLE(EgoVehicle->ReleaseTurnSignalR())
}

void ADReyeVRPawn::PressHandbrake()
{
    CHECK_EGO_VEHICLE(EgoVehicle->PressHandbrake())
}

void ADReyeVRPawn::ReleaseHandbrake()
{
    CHECK_EGO_VEHICLE(EgoVehicle->ReleaseHandbrake())
}

void ADReyeVRPawn::CameraFwd()
{
    CHECK_EGO_VEHICLE(EgoVehicle->CameraFwd())
}

void ADReyeVRPawn::CameraBack()
{
    CHECK_EGO_VEHICLE(EgoVehicle->CameraBack())
}

void ADReyeVRPawn::CameraLeft()
{
    CHECK_EGO_VEHICLE(EgoVehicle->CameraLeft())
}

void ADReyeVRPawn::CameraRight()
{
    CHECK_EGO_VEHICLE(EgoVehicle->CameraRight())
}

void ADReyeVRPawn::CameraUp()
{
    CHECK_EGO_VEHICLE(EgoVehicle->CameraUp())
}

void ADReyeVRPawn::CameraDown()
{
    CHECK_EGO_VEHICLE(EgoVehicle->CameraDown())
}


void ADReyeVRPawn::InitLogiWheel()
{
#if USE_LOGITECH_PLUGIN
    LogiSteeringInitialize(false);
    bIsLogiConnected = LogiIsConnected(WheelDeviceIdx); // get status of connected device
    if (bIsLogiConnected)
    {
        const size_t n = 1000; // name shouldn't be more than 1000 chars right?
        wchar_t *NameBuffer = (wchar_t *)malloc(n * sizeof(wchar_t));
        if (LogiGetFriendlyProductName(WheelDeviceIdx, NameBuffer, n) == false)
        {
            UE_LOG(LogTemp, Warning, TEXT("Unable to get Logi friendly name!"));
            NameBuffer = L"Unknown";
        }
        std::wstring wNameStr(NameBuffer, n);
        std::string NameStr(wNameStr.begin(), wNameStr.end());
        FString LogiName(NameStr.c_str());
        UE_LOG(LogTemp, Log, TEXT("Found a Logitech device (%s) connected on input %d"), *LogiName, WheelDeviceIdx);
        free(NameBuffer); // no longer needed
    }
    else
    {
        const FString LogiError = "Could not find Logitech device connected on input 0";
        const bool PrintToLog = false;
        const bool PrintToScreen = true;
        const float ScreenDurationSec = 20.f;
        const FLinearColor MsgColour = FLinearColor(1, 0, 0, 1); // RED
        UKismetSystemLibrary::PrintString(World, LogiError, PrintToScreen, PrintToLog, MsgColour, ScreenDurationSec);
        UE_LOG(LogTemp, Error, TEXT("%s"), *LogiError); // Error is RED
    }
#endif
}

void ADReyeVRPawn::DestroyLogiWheel(bool DestroyModule)
{
#if USE_LOGITECH_PLUGIN
    if (bIsLogiConnected)
    {
        // stop any forces on the wheel (we only use spring force feedback)
        LogiStopSpringForce(WheelDeviceIdx);

        if (DestroyModule) // only destroy the module at the end of the game (not ego life)
        {
            // shutdown the entire module (dangerous bc lingering pointers)
            LogiSteeringShutdown();
        }
    }
#endif
}

void ADReyeVRPawn::TickLogiWheel()
{
#if USE_LOGITECH_PLUGIN
    bIsLogiConnected = LogiIsConnected(WheelDeviceIdx); // get status of connected device
    EgoVehicle->bIsLogiConnected = bIsLogiConnected; // TODO: cleanup
    if (bIsLogiConnected)
    {
        // Taking logitech inputs for steering
        LogitechWheelUpdate();

        // Add Force Feedback to the hardware steering wheel when a LogitechWheel is used
        ApplyForceFeedback();
    }
#endif
}

#if USE_LOGITECH_PLUGIN

// const std::vector<FString> VarNames = {"rgdwPOV[0]", "rgdwPOV[1]", "rgdwPOV[2]", "rgdwPOV[3]"};
const std::vector<FString> VarNames = {                                                    // 34 values
    "lX",           "lY",           "lZ",         "lRz",           "lRy",           "lRz", // variable names
    "rglSlider[0]", "rglSlider[1]", "rgdwPOV[0]", "rgbButtons[0]", "lVX",           "lVY",           "lVZ",
    "lVRx",         "lVRy",         "lVRz",       "rglVSlider[0]", "rglVSlider[1]", "lAX",           "lAY",
    "lAZ",          "lARx",         "lARy",       "lARz",          "rglASlider[0]", "rglASlider[1]", "lFX",
    "lFY",          "lFZ",          "lFRx",       "lFRy",          "lFRz",          "rglFSlider[0]", "rglFSlider[1]"};
/// NOTE: this is a debug function used to dump all the information we can regarding
// the Logitech wheel hardware we used since the exact buttons were not documented in
// the repo: https://github.com/HARPLab/LogitechWheelPlugin
void ADReyeVRPawn::LogLogitechPluginStruct(const struct DIJOYSTATE2 *Now)
{
    if (Old == nullptr)
    {
        Old = new struct DIJOYSTATE2;
        (*Old) = (*Now); // assign to the new (current) dijoystate struct
        return;          // initializing the Old struct ptr
    }
    const std::vector<int> NowVals = {
        Now->lX, Now->lY, Now->lZ, Now->lRx, Now->lRy, Now->lRz, Now->rglSlider[0], Now->rglSlider[1],
        // Converting unsigned int & unsigned char to int
        int(Now->rgdwPOV[0]), int(Now->rgbButtons[0]), Now->lVX, Now->lVY, Now->lVZ, Now->lVRx, Now->lVRy, Now->lVRz,
        Now->rglVSlider[0], Now->rglVSlider[1], Now->lAX, Now->lAY, Now->lAZ, Now->lARx, Now->lARy, Now->lARz,
        Now->rglASlider[0], Now->rglASlider[1], Now->lFX, Now->lFY, Now->lFZ, Now->lFRx, Now->lFRy, Now->lFRz,
        Now->rglFSlider[0], Now->rglFSlider[1]}; // 32 elements
    // Getting the (34) values from the old struct
    const std::vector<int> OldVals = {
        Old->lX, Old->lY, Old->lZ, Old->lRx, Old->lRy, Old->lRz, Old->rglSlider[0], Old->rglSlider[1],
        // Converting unsigned int & unsigned char to int
        int(Old->rgdwPOV[0]), int(Old->rgbButtons[0]), Old->lVX, Old->lVY, Old->lVZ, Old->lVRx, Old->lVRy, Old->lVRz,
        Old->rglVSlider[0], Old->rglVSlider[1], Old->lAX, Old->lAY, Old->lAZ, Old->lARx, Old->lARy, Old->lARz,
        Old->rglASlider[0], Old->rglASlider[1], Old->lFX, Old->lFY, Old->lFZ, Old->lFRx, Old->lFRy, Old->lFRz,
        Old->rglFSlider[0], Old->rglFSlider[1]};

    check(NowVals.size() == OldVals.size() && NowVals.size() == VarNames.size());

    // print any differences
    bool isDiff = false;
    for (size_t i = 0; i < NowVals.size(); i++)
    {
        if (NowVals[i] != OldVals[i])
        {
            if (!isDiff) // only gets triggered at MOST once
            {
                UE_LOG(LogTemp, Log, TEXT("Logging joystick at t=%.3f"), UGameplayStatics::GetRealTimeSeconds(World));
                isDiff = true;
            }
            UE_LOG(LogTemp, Log, TEXT("Triggered \"%s\" from %d to %d"), *(VarNames[i]), OldVals[i], NowVals[i]);
        }
    }

    // also check the 128 rgbButtons array
    for (size_t i = 0; i < 127; i++)
    {
        if (Old->rgbButtons[i] != Now->rgbButtons[i])
        {
            if (!isDiff) // only gets triggered at MOST once
            {
                UE_LOG(LogTemp, Log, TEXT("Logging joystick at t=%.3f"), UGameplayStatics::GetRealTimeSeconds(World));
                isDiff = true;
            }
            UE_LOG(LogTemp, Log, TEXT("Triggered \"rgbButtons[%d]\" from %d to %d"), int(i), int(OldVals[i]),
                   int(NowVals[i]));
        }
    }

    // assign the current joystate into the old one
    (*Old) = (*Now);
}

void ADReyeVRPawn::LogitechWheelUpdate()
{
    if (EgoVehicle == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("No egovehicle!"));
        return;
    }
    // only execute this in Windows, the Logitech plugin is incompatible with Linux
    if (LogiUpdate() == false) // update the logitech wheel
        UE_LOG(LogTemp, Warning, TEXT("Logitech wheel %d failed to update!"), WheelDeviceIdx);
    DIJOYSTATE2 *WheelState = LogiGetState(WheelDeviceIdx);
    if (bLogLogitechWheel)
        LogLogitechPluginStruct(WheelState);
    /// NOTE: obtained these from LogitechWheelInputDevice.cpp:~111
    // -32768 to 32767. -32768 = all the way to the left. 32767 = all the way to the right.
    const float WheelRotation = FMath::Clamp(float(WheelState->lX), -32767.0f, 32767.0f) / 32767.0f; // (-1, 1)
    // -32768 to 32767. 32767 = pedal not pressed. -32768 = pedal fully pressed.
    const float AccelerationPedal = fabs(((WheelState->lY - 32767.0f) / (65535.0f))); // (0, 1)
    // -32768 to 32767. Higher value = less pressure on brake pedal
    const float BrakePedal = fabs(((WheelState->lRz - 32767.0f) / (65535.0f))); // (0, 1)
    // -1 = not pressed. 0 = Top. 0.25 = Right. 0.5 = Bottom. 0.75 = Left.
    const float Dpad = fabs(((WheelState->rgdwPOV[0] - 32767.0f) / (65535.0f)));

    // weird behaviour: "Pedals will output a value of 0.5 until the wheel/pedals receive any kind of input"
    // as per https://github.com/HARPLab/LogitechWheelPlugin
    if (EgoVehicle->bPedalsDefaulting)
    {
        if (!(FMath::IsNearlyEqual(WheelRotation, 0.f, 0.01f) && FMath::IsNearlyEqual(AccelerationPedal, 0.5f, 0.01f) &&
              FMath::IsNearlyEqual(BrakePedal, 0.5f, 0.01f)))
        {
            EgoVehicle->bPedalsDefaulting = false;
        }
    }
    else
    {
        /// (NOTE: these function calls occur in the EgoVehicle::Tick, meaning other
        // control calls could theoretically conflict/override them if called later. This is
        // the case with the keyboard inputs which is why there are wrappers (suffixed with "Kbd")
        // that always override logi inputs IF their values are nonzero
        EgoVehicle->SetSteering(WheelRotation);
        EgoVehicle->SetThrottle(AccelerationPedal);
        EgoVehicle->SetBrake(BrakePedal);
    }

    //    UE_LOG(LogTemp, Log, TEXT("Dpad value %f"), Dpad);
    //    if (WheelState->rgdwPOV[0] == 0) // should work now

    // Button presses (turn signals, reverse)
    if (WheelState->rgbButtons[0] || WheelState->rgbButtons[1] || // Any of the 4 face pads
        WheelState->rgbButtons[2] || WheelState->rgbButtons[3])
        EgoVehicle->PressReverse();
    else
        EgoVehicle->ReleaseReverse();

    if (WheelState->rgbButtons[4])
        EgoVehicle->PressTurnSignalR();
    else
        EgoVehicle->ReleaseTurnSignalR();

    if (WheelState->rgbButtons[5])
        EgoVehicle->PressTurnSignalL();
    else
        EgoVehicle->ReleaseTurnSignalL();

    // if (WheelState->rgbButtons[23]) // big red button on right side of g923

    // VRCamerRoot base position adjustment
    if (WheelState->rgdwPOV[0] == 0) // positive in X
        EgoVehicle->CameraPositionAdjust(FVector(1.f, 0.f, 0.f));
    else if (WheelState->rgdwPOV[0] == 18000) // negative in X
        EgoVehicle->CameraPositionAdjust(FVector(-1.f, 0.f, 0.f));
    else if (WheelState->rgdwPOV[0] == 9000) // positive in Y
        EgoVehicle->CameraPositionAdjust(FVector(0.f, 1.f, 0.f));
    else if (WheelState->rgdwPOV[0] == 27000) // negative in Y
        EgoVehicle->CameraPositionAdjust(FVector(0.f, -1.f, 0.f));
    // VRCamerRoot base height adjustment
    else if (WheelState->rgbButtons[19]) // positive in Z
        EgoVehicle->CameraPositionAdjust(FVector(0.f, 0.f, 1.f));
    else if (WheelState->rgbButtons[20]) // negative in Z
        EgoVehicle->CameraPositionAdjust(FVector(0.f, 0.f, -1.f));
}

void ADReyeVRPawn::ApplyForceFeedback() const
{
    // only execute this in Windows, the Logitech plugin is incompatible with Linux
    const float Speed = EgoVehicle->GetVelocity().Size(); // get magnitude of self (AActor's) velocity
    /// TODO: move outside this function (in tick()) to avoid redundancy
    if (EgoVehicle->bIsLogiConnected && LogiHasForceFeedback(WheelDeviceIdx))
    {
        const int OffsetPercentage = 0;      // "Specifies the center of the spring force effect"
        const int SaturationPercentage = 30; // "Level of saturation... comparable to a magnitude"
        const int CoeffPercentage = 100; // "Slope of the effect strength increase relative to deflection from Offset"
        LogiPlaySpringForce(WheelDeviceIdx, OffsetPercentage, SaturationPercentage, CoeffPercentage);
    }
    else
    {
        LogiStopSpringForce(WheelDeviceIdx);
    }
    /// NOTE: there are other kinds of forces as described in the LogitechWheelPlugin API:
    // https://github.com/HARPLab/LogitechWheelPlugin/blob/master/LogitechWheelPlugin/Source/LogitechWheelPlugin/Private/LogitechBWheelInputDevice.cpp
    // For example:
    /*
        Force Types
        0 = Spring				5 = Dirt Road
        1 = Constant			6 = Bumpy Road
        2 = Damper				7 = Slippery Road
        3 = Side Collision		8 = Surface Effect
        4 = Frontal Collision	9 = Car Airborne
    */
}
#endif