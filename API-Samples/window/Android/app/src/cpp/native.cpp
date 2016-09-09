#include "native.h"

android_app* Android_App=0;                //Android native-actvity state

//--------------------TEMP------------------------
//--Window event handler--
static void handle_cmd(struct android_app* app, int32_t cmd) {
    //printf(" -> handle_cmd");
    switch(cmd){
        case APP_CMD_INPUT_CHANGED  : printf("APP_CMD_INPUT_CHANGED");  break;
        case APP_CMD_INIT_WINDOW    : printf("APP_CMD_INIT_WINDOW");    break;
        case APP_CMD_TERM_WINDOW    : printf("APP_CMD_TERM_WINDOW");    break;
        //case APP_CMD_WINDOW_RESIZED       : printf("APP_CMD_WINDOW_RESIZED");       break;
        //case APP_CMD_WINDOW_REDRAW_NEEDED : printf("APP_CMD_WINDOW_REDRAW_NEEDED"); break;
        //case APP_CMD_CONTENT_RECT_CHANGED : printf("APP_CMD_CONTENT_RECT_CHANGED"); break;
        case APP_CMD_GAINED_FOCUS   : printf("APP_CMD_GAINED_FOCUS");   break;
        case APP_CMD_LOST_FOCUS     : printf("APP_CMD_LOST_FOCUS");     break;
        case APP_CMD_CONFIG_CHANGED : printf("APP_CMD_CONFIG_CHANGED"); break;
        case APP_CMD_LOW_MEMORY     : printf("APP_CMD_LOW_MEMORY");     break;
        case APP_CMD_START          : printf("APP_CMD_START");          break;
        case APP_CMD_RESUME         : printf("APP_CMD_RESUME");         break;
        case APP_CMD_SAVE_STATE     : printf("APP_CMD_SAVE_STATE");     break;
        case APP_CMD_PAUSE          : printf("APP_CMD_PAUSE");          break;
        case APP_CMD_STOP           : printf("APP_CMD_STOP");           break;
        case APP_CMD_DESTROY        : printf("APP_CMD_DESTROY");        break;
        default : printf("handle_cmd : UNKNOWN EVENT");
    }
}

//--Input event handler--
static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    //printf(" -> handle_input\n");
    int32_t type=AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        float mx = AMotionEvent_getX(event, 0);
        float my = AMotionEvent_getY(event, 0);
        printf("%f x %f\n",mx,my);
        return 1;
    }
    return 0;
}
//------------------------------------------------

//====================Main====================
int main(int argc, char *argv[]);          //Forward declaration of main function

void android_main(struct android_app* state) {
    printf("Native Activity\n");
    app_dummy();                           // Make sure glue isn't stripped
    state->onAppCmd     = handle_cmd;      // Register window event callback
    //state->onInputEvent = handle_input;    // Register input event callback
    Android_App=state;                     // Pass android app state to window_andoid.cpp

    int success=InitVulkan();
    printf("InitVulkan : %s\n",success ? "SUCCESS" : "FAILED");

    main(0,NULL);

    printf("Exiting.\n");
    ANativeActivity_finish(state->activity);
}
//============================================

//========================UGLY JNI code for showing the Keyboard========================

#define CALL_OBJ_METHOD( OBJ,METHOD,SIGNATURE, ...) JNIEnv->CallObjectMethod (OBJ, JNIEnv->GetMethodID(JNIEnv->GetObjectClass(OBJ),METHOD,SIGNATURE), __VA_ARGS__)
#define CALL_BOOL_METHOD(OBJ,METHOD,SIGNATURE, ...) JNIEnv->CallBooleanMethod(OBJ, JNIEnv->GetMethodID(JNIEnv->GetObjectClass(OBJ),METHOD,SIGNATURE), __VA_ARGS__)

void ShowKeyboard(bool visible,int flags){
    // Attach current thread to the JVM.
    JavaVM* JavaVM = Android_App->activity->vm;
    JNIEnv* JNIEnv = Android_App->activity->env;
    JavaVMAttachArgs Args={JNI_VERSION_1_6, "NativeThread", NULL};
    JavaVM->AttachCurrentThread(&JNIEnv, &Args);

    // Retrieve NativeActivity.
    jobject lNativeActivity = Android_App->activity->clazz;

    // Retrieve Context.INPUT_METHOD_SERVICE.
    jclass ClassContext = JNIEnv->FindClass("android/content/Context");
    jfieldID FieldINPUT_METHOD_SERVICE =JNIEnv->GetStaticFieldID(ClassContext, "INPUT_METHOD_SERVICE", "Ljava/lang/String;");
    jobject INPUT_METHOD_SERVICE =JNIEnv->GetStaticObjectField(ClassContext, FieldINPUT_METHOD_SERVICE);

    // getSystemService(Context.INPUT_METHOD_SERVICE).
    jobject   lInputMethodManager = CALL_OBJ_METHOD(lNativeActivity, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", INPUT_METHOD_SERVICE);

    // getWindow().getDecorView().
    jobject   lWindow    = CALL_OBJ_METHOD (lNativeActivity,"getWindow", "()Landroid/view/Window;",0);
    jobject   lDecorView = CALL_OBJ_METHOD (lWindow, "getDecorView", "()Landroid/view/View;",0);
    if (visible) {
        jboolean lResult = CALL_BOOL_METHOD(lInputMethodManager, "showSoftInput", "(Landroid/view/View;I)Z", lDecorView, flags);
    } else {
        jobject  lBinder = CALL_OBJ_METHOD (lDecorView, "getWindowToken", "()Landroid/os/IBinder;",0);
        jboolean lResult = CALL_BOOL_METHOD(lInputMethodManager, "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z", lBinder, flags);
    }
    // Finished with the JVM.
    JavaVM->DetachCurrentThread();
}

//======================================================================================

//===============================Get Unicode from Keyboard==============================

int GetUnicodeChar(struct android_app* app, int eventType, int keyCode, int metaState)
{
    JavaVM* javaVM = app->activity->vm;
    JNIEnv* jniEnv = app->activity->env;

    JavaVMAttachArgs attachArgs;
    attachArgs.version = JNI_VERSION_1_6;
    attachArgs.name = "NativeThread";
    attachArgs.group = NULL;

    jint result = javaVM->AttachCurrentThread(&jniEnv, &attachArgs);
    if(result == JNI_ERR) return 0;


    jclass class_key_event = jniEnv->FindClass("android/view/KeyEvent");
    int unicodeKey;

    if(metaState == 0){
        jmethodID method_get_unicode_char = jniEnv->GetMethodID(class_key_event, "getUnicodeChar", "()I");
        jmethodID eventConstructor = jniEnv->GetMethodID(class_key_event, "<init>", "(II)V");
        jobject eventObj = jniEnv->NewObject(class_key_event, eventConstructor, eventType, keyCode);
        unicodeKey = jniEnv->CallIntMethod(eventObj, method_get_unicode_char);
    }else{
        jmethodID method_get_unicode_char = jniEnv->GetMethodID(class_key_event, "getUnicodeChar", "(I)I");
        jmethodID eventConstructor = jniEnv->GetMethodID(class_key_event, "<init>", "(II)V");
        jobject eventObj = jniEnv->NewObject(class_key_event, eventConstructor, eventType, keyCode);
        unicodeKey = jniEnv->CallIntMethod(eventObj, method_get_unicode_char, metaState);
    }

    javaVM->DetachCurrentThread();

    LOGI("Unicode key is: %d", unicodeKey);
    return unicodeKey;
}


//======================================================================================