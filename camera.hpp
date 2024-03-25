#pragma once

#include "math.hpp"
#include "cbuffer.hpp"

class Camera
{
	
public:

	struct CbParam
	{
		CbParam() : mView(), mProj() {}
		XMFLOAT4X4 mView;
		XMFLOAT4X4 mProj;
	};

	explicit Camera(const Vec3f& position, const Vec3f& lookAt, float aspectRatio, HWND hWnd) :
		mPosition(position.x, position.y, position.z),
		mForward(lookAt.x, lookAt.y, lookAt.z),
		mUp(0, 1, 0),
		mAspectRatio(aspectRatio),
		mMoveStep(15.0f),
		mRotStep(0.1f),
		mLastX(0),
		mLastY(0),
		mDamping(1.0f),
		mNear(0.001f),
		mFov(XM_PIDIV4),
		mFar(500),
		mPhi(0),
		mTheta(0),
		hWnd(hWnd)
	{
		// hack for fps camera -> will change with scene
		mForward.x -= mPosition.x;
		mForward.y -= mPosition.y;
		mForward.z -= mPosition.z;
		XMVECTOR forward = XMLoadFloat3(&mForward);
		XMStoreFloat3(&mForward, XMVector3Normalize(forward));
		
		// get angles from initial direction
		float pitch = XMScalarASin(mForward.y);
		float yaw = XMScalarACos(mForward.x / XMScalarCos(pitch));
		mYaw = XMConvertToDegrees(yaw);
		mPitch = XMConvertToDegrees(pitch);

		mCurrentForward = mForward;
		mCurrentPosition = mPosition;
		UpdateViewMatrix();
		UpdateProjMatrix();
	}

	~Camera()
	{}
	
	bool Create(ID3D11Device* Device)
	{
		if (!mParam.Create(Device)) return false;
		return true;
	}

	void Release()
	{
		mParam.Release();
	}

	void Update(double elapsedTime)
	{
		POINT arg;
		if (!GetCursorPos(&arg))
		{
			return;
		}

		bool window_has_focus = GetActiveWindow() == hWnd;
		if (!window_has_focus) 
		{
			// fix camera jumping
			mLastX = arg.x;
			mLastY = arg.y;
			return; // for now we want this for interactivity
		}

		UpdatePosition(elapsedTime);

		
		if (GetAsyncKeyState(VK_RBUTTON) < 0)
		{
			float rx = (arg.x - mLastX) * mRotStep;
			float ry = -(arg.y - mLastY) * mRotStep;
			if (rx != 0 || ry != 0)
			{
				mYaw -= rx;
				mPitch += ry;
				if (mPitch > 89.0f)
					mPitch = 89.0f;
				if (mPitch < -89.0f)
					mPitch = -89.0f;

				XMFLOAT3 direction;
				float radianYaw = XMConvertToRadians(mYaw);
				float radianPitch = XMConvertToRadians(mPitch);
				direction.x = XMScalarCos(radianYaw) * XMScalarCos(radianPitch);
				direction.y = XMScalarSin(radianPitch);
				direction.z = XMScalarSin(radianYaw) * XMScalarCos(radianPitch);
				XMVECTOR forward = XMLoadFloat3(&direction);
				XMStoreFloat3(&mForward, XMVector3Normalize(forward));

				UpdateViewMatrix();
				SetCursorPos(mLastX, mLastY);
			}
		}
		else
		{
			mLastX = arg.x;
			mLastY = arg.y;
		}
		

		UpdateViewMatrix();
		UpdateProjMatrix();
	}
	
	ConstantBuffer<CbParam>& GetParams() { return mParam; }

	float GetCameraSpeed() { return mMoveStep; }
	void SetCameraSpeed(float value) { mMoveStep = value; }

	float GetFov() { return mFov; }
	void SetFov(float value) { mFov = value; }

	float GetNear() { return mNear; }
	void SetNear(float value) { mNear = value; }

	float GetFar() { return mFar; }
	void SetFar(float value) { mFar = value; }
	
	XMFLOAT3 GetForward() { return mForward; }
	void SetForward(XMFLOAT3& forward) { mForward = forward; }
private:


	void UpdatePosition(double elapsedTime)
	{
		// forward / backward
		float y = (float)((GetAsyncKeyState('W') < 0) - (GetAsyncKeyState('S') < 0)) * mMoveStep * (float)elapsedTime;
		if (y != 0)
		{
			XMVECTOR eye = XMLoadFloat3(&mPosition);
			XMVECTOR forward = XMLoadFloat3(&mForward);
			eye = eye + forward * y;
			XMStoreFloat3(&mPosition, eye);
		}

		// left / right
		y = (float)((GetAsyncKeyState('D') < 0) - (GetAsyncKeyState('A') < 0)) * mMoveStep * (float)elapsedTime;
		if (y != 0)
		{
			XMVECTOR eye = XMLoadFloat3(&mPosition);
			XMVECTOR forward = XMLoadFloat3(&mForward);
			XMVECTOR up = XMLoadFloat3(&mUp);
			XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
			eye = eye + right * y;
			XMStoreFloat3(&mPosition, eye);
		}

		// up / down
		y = (float)((GetAsyncKeyState(VK_SPACE) < 0) - (GetAsyncKeyState(VK_LCONTROL) < 0)) * mMoveStep * (float)elapsedTime;
		if (y != 0)
		{
			XMVECTOR eye = XMLoadFloat3(&mPosition);
			XMVECTOR up = XMLoadFloat3(&mUp);
			eye = eye + up * y;
			XMStoreFloat3(&mPosition, eye);
		}

	}

	void UpdateViewMatrix()
	{
		mCurrentPosition.x += (mPosition.x - mCurrentPosition.x) * mDamping;
		mCurrentPosition.y += (mPosition.y - mCurrentPosition.y) * mDamping;
		mCurrentPosition.z += (mPosition.z - mCurrentPosition.z) * mDamping;

		mCurrentForward.x += (mForward.x - mCurrentForward.x) * mDamping;
		mCurrentForward.y += (mForward.y - mCurrentForward.y) * mDamping;
		mCurrentForward.z += (mForward.z - mCurrentForward.z) * mDamping;

		XMVECTOR position = XMLoadFloat3(&mCurrentPosition);
		XMVECTOR lookAt = XMLoadFloat3(&mCurrentForward);
		XMVECTOR up = XMLoadFloat3(&mUp);
		XMMATRIX view = XMMatrixLookAtLH(
			position,
			position + lookAt,
			up);
		XMStoreFloat4x4(&mParam.Data.mView, view);
	}

	void UpdateProjMatrix()
	{
		XMMATRIX proj = XMMatrixPerspectiveFovLH(mFov, mAspectRatio, mNear, mFar);
		XMStoreFloat4x4(&mParam.Data.mProj, proj);
	}

	HWND hWnd;

	XMFLOAT3 mPosition;
	XMFLOAT3 mForward;
	XMFLOAT3 mUp;

	ConstantBuffer<CbParam> mParam;

	XMFLOAT3 mCurrentForward;
	XMFLOAT3 mCurrentPosition;

	float mMoveStep;
	float mRotStep;
	int mLastX;
	int mLastY;
	float mAspectRatio;
	float mPhi;
	float mTheta;

	float mDamping;

	float mNear;
	float mFar;
	float mFov;

	float mYaw;
	float mPitch;
};
