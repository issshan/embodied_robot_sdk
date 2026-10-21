#if UNITY_EDITOR
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.XR.Management;
using UnityEditor.XR.Management;
using UnityEditor.XR.Management.Metadata;
using UnityEngine.XR.OpenXR;
using UnityEngine.XR.OpenXR.Features.Interactions;
using UnityEngine.XR.OpenXR.Features.OculusQuestSupport;
namespace QuestRobotTeleop {
public static class SceneBuilder {
 [MenuItem("Teleop/Create Desktop Simulator Scene")]
 public static void Desktop(){Create(true);}
 [MenuItem("Teleop/Create Quest Scene")]
 public static void Quest(){Create(false);}
 static void Create(bool desktop){
  EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
  var app=new GameObject("Teleop App").AddComponent<QuestTeleopApp>();app.desktopSimulation=desktop;
  var camera=new GameObject("XR Camera");camera.tag="MainCamera";camera.AddComponent<Camera>();camera.AddComponent<AudioListener>();
  var light=new GameObject("Light").AddComponent<Light>();light.type=LightType.Directional;light.transform.rotation=Quaternion.Euler(50,-30,0);
  var floor=GameObject.CreatePrimitive(PrimitiveType.Plane);floor.transform.position=new Vector3(0,-0.02f,0);floor.transform.localScale=Vector3.one*0.2f;
  System.IO.Directory.CreateDirectory("Assets/Scenes");
  string path=desktop?"Assets/Scenes/Desktop.unity":"Assets/Scenes/Quest.unity";
  EditorSceneManager.SaveScene(EditorSceneManager.GetActiveScene(),path);EditorBuildSettings.scenes=new[]{new EditorBuildSettingsScene(path,true)};
  Selection.activeGameObject=app.gameObject;
 }
 [MenuItem("Teleop/Configure Android OpenXR")]
 public static void ConfigureQuest(){
  PlayerSettings.companyName="QuestRobotTeleop";PlayerSettings.productName="Quest Robot Teleop";
  PlayerSettings.SetApplicationIdentifier(UnityEditor.Build.NamedBuildTarget.Android,"com.issshan.questrobotteleop");
  PlayerSettings.SetScriptingBackend(UnityEditor.Build.NamedBuildTarget.Android,ScriptingImplementation.IL2CPP);
  PlayerSettings.Android.targetArchitectures=AndroidArchitecture.ARM64;
  PlayerSettings.Android.minSdkVersion=AndroidSdkVersions.AndroidApiLevel32;
  PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.Android,false);PlayerSettings.SetGraphicsAPIs(BuildTarget.Android,new[]{GraphicsDeviceType.Vulkan});
  var serialized=new SerializedObject(Unsupported.GetSerializedAssetInterfaceSingleton("PlayerSettings"));
  var input=serialized.FindProperty("activeInputHandler");if(input!=null){input.intValue=1;serialized.ApplyModifiedProperties();}
  var group=BuildTargetGroup.Android;
  var settings=XRGeneralSettingsPerBuildTarget.XRGeneralSettingsForBuildTarget(group);
  if(settings==null){Debug.LogWarning("Open Project Settings > XR Plug-in Management, initialize Android settings, then rerun Configure Android OpenXR.");return;}
  XRPackageMetadataStore.AssignLoader(settings.Manager,"UnityEngine.XR.OpenXR.OpenXRLoader",group);
  var openxr=OpenXRSettings.GetSettingsForBuildTargetGroup(group);
  if(openxr!=null){var touch=openxr.GetFeature<OculusTouchControllerProfile>();if(touch!=null)touch.enabled=true;var quest=openxr.GetFeature<OculusQuestFeature>();if(quest!=null)quest.enabled=true;EditorUtility.SetDirty(openxr);}
  AssetDatabase.SaveAssets();Debug.Log("Review Android OpenXR Project Validation before building. Quest 3S hardware validation is still required.");
 }
}
}
#endif
