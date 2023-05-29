#ifndef CAMERA_H
#define CAMERA_H

#include "pch.h"
#include "dxHelper.h"

class Camera
{
public:
	Camera();
	~Camera();

	XMVECTOR getPosition() const;
	XMFLOAT3 getPosition3f() const;
	void setPosition(float x, float y, float z);
	void setPosition(const XMFLOAT3& v);

	XMVECTOR getRight() const;
	XMFLOAT3 getRight3f() const;
	XMVECTOR getUp() const;
	XMFLOAT3 getUp3f() const;
	XMVECTOR getLook() const;
	XMFLOAT3 getLook3f() const;

	float getNearZ() const;
	float getFarZ() const;
	float getAspect() const;
	float getFovY() const;
	float getFovX() const;

	float getNearWindowWidth() const;
	float getNearWindowHeight() const;
	float getFarWindowWidth() const;
	float getFarWindowHeight() const;

	bool notifyChanged()
	{
		if (mViewDirty)
		{
			mViewDirty = false;
			return true;
		}
		return false;
	}

	void setLens(float fovY, float aspect, float zn, float zf);

	void lookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp);
	void lookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up);

	XMMATRIX getView() const;
	XMMATRIX getProj() const;

	XMFLOAT4X4 getView4x4f() const;
	XMFLOAT4X4 getProj4x4f() const;

	void strafe(float ds);
	void walk(float d);

	void pitch(float angle);
	void rotateY(float angle);

	void executeKeyboard();
	void updateViewMatrix();
	void update();

private:
	XMFLOAT3 mPosition = { -2.0f, 2.0f, -1.f };
	XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
	XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
	XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

	float mNearZ = 0.0f;
	float mFarZ = 0.0f;
	float mAspect = 0.0f;
	float mFovY = 0.0f;
	float mNearWindowHeight = 0.0f;
	float mFarWindowHeight = 0.0f;

	bool mViewDirty = true;

	XMFLOAT4X4 mView = IdentityMatrix4x4();
	XMFLOAT4X4 mProj = IdentityMatrix4x4();
};

#endif