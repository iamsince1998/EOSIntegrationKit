// Copyright (c) 2023 Betide Studio. All Rights Reserved.


#include "GoogleLogin_SLK.h"
#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h" 
#include "Android/Utils/AndroidJNICallUtils.h"
#include "Android/Utils/AndroidJNIConvertor.h"
#include "Runtime/Core/Public/Async/TaskGraphInterfaces.h"
#endif

#if PLATFORM_IOS
#include "Async/Async.h"
#define GTM_APPAUTH_SKIP_GOOGLE_SUPPORT 0
#define GTMSESSION_ASSERT_AS_LOG 1

#pragma clang diagnostic ignored "-Wobjc-property-no-attribute" // Ignore errors generated by FBSDKCoreKit.h and GoogleSignIn.h
#import <Foundation/Foundation.h>
#include "IOSAppDelegate.h"
#import <GoogleSignIn/GoogleSignIn.h>
#import <AuthenticationServices/AuthenticationServices.h>
#import <SafariServices/SafariServices.h>
#endif

TWeakObjectPtr<UGoogleLogin_SLK> UGoogleLogin_SLK::staticInstance = nullptr;

UGoogleLogin_SLK* UGoogleLogin_SLK::GoogleLogin(UObject* WorldContextObject, const FString& ClientID)
{
	UGoogleLogin_SLK* Action = NewObject<UGoogleLogin_SLK>();
	Action->Var_ClientID = ClientID;
	return Action;
}

#if PLATFORM_IOS
static void OnGoogleOpenURL(UIApplication* application, NSURL* url, NSString* sourceApplication, id annotation)
{
	BOOL handled;
	handled = [GIDSignIn.sharedInstance handleURL:url];
}
#endif

void UGoogleLogin_SLK::Activate()
{
	UGoogleLogin_SLK::staticInstance = this;

#if PLATFORM_IOS
	FIOSCoreDelegates::OnOpenURL.AddStatic(&OnGoogleOpenURL);
#endif
	
	GoogleLoginLocal();
	Super::Activate();
}

void UGoogleLogin_SLK::BeginDestroy()
{
	UGoogleLogin_SLK::staticInstance = nullptr;
	Super::BeginDestroy();
}

void UGoogleLogin_SLK::GoogleLoginLocal()
{
#if PLATFORM_ANDROID
	AndroidJNICallUtils::CallGameActivityVoidMethod("GameActivity","AndroidThunkJava_GoogleSubsystem_Init", "()V");
	AndroidJNICallUtils::CallGameActivityVoidMethod("GameActivity", "AndroidThunkJava_GoogleSubsystem_SignIn", "(Ljava/lang/String;)V", AndroidJNIConvertor::GetJavaString(Var_ClientID));
	return;
#endif
#if PLATFORM_IOS
	UIViewController *viewController = [IOSAppDelegate GetDelegate].IOSController;

	[GIDSignIn.sharedInstance signInWithPresentingViewController:viewController
													  completion:^(GIDSignInResult* result, NSError* signInError) {
		if (signInError == nil) {
			const char* token_str = [result.user.accessToken.tokenString UTF8String];
			AsyncTask(ENamedThreads::GameThread, [this, token_str]() {
				UGoogleLogin_SLK::staticInstance.Get()->Success.Broadcast(FString(token_str), "");
			});
		} else {
			// Handle sign-in error
			const char* c_error = [signInError.localizedDescription UTF8String];
			AsyncTask(ENamedThreads::GameThread, [this, c_error]() {
			   UGoogleLogin_SLK::staticInstance.Get()->Failure.Broadcast("", FString(c_error));
			});
		}
	}];
	return;
#endif
	UGoogleLogin_SLK::staticInstance.Get()->Failure.Broadcast("", "Google Login Failed to initialise");
}

#if PLATFORM_ANDROID
extern "C"
{
	JNIEXPORT void Java_com_epicgames_unreal_GameActivity_OnSignInResult(JNIEnv* env, jclass clazz, jboolean success, jstring response, jstring token)
	{
		FString responseStr = AndroidJNIConvertor::FromJavaString(response);
		FString tokenStr = AndroidJNIConvertor::FromJavaString(token);
		if (UGoogleLogin_SLK::staticInstance.Get())
		{
			AsyncTask(ENamedThreads::GameThread, [success, responseStr, tokenStr]() {
				if (success)
				{
					UGoogleLogin_SLK::staticInstance.Get()->Success.Broadcast(tokenStr, responseStr);
				}
				else
				{
					UGoogleLogin_SLK::staticInstance.Get()->Failure.Broadcast("", responseStr);
				}
			});
		}
	}
	JNIEXPORT void Java_com_epicgames_unreal_GameActivity_OnSignOutResult(JNIEnv* env, jclass clazz, jboolean success, jstring response)
	{
		FString responseStr = AndroidJNIConvertor::FromJavaString(response);
		
		//....
	}
}
#endif
