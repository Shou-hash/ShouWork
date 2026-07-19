#pragma once

struct Vector3
{
	float x;
	float y;
	float z;
};

struct Matrix4x4
{
	float m[4][4];
};

Matrix4x4 MakeIdentity4x4();

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translation);

Matrix4x4 Inverse(const Matrix4x4& m);
Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b);
Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspect, float nearClip, float farClip);
Matrix4x4 MakeOrthographicMatrix(float left, float right, float top, float bottom, float nearClip, float farClip);
Matrix4x4 MakeScaleMatrix(const Vector3& scale);
Matrix4x4 MakeRotateXMatrix(float angle);
Matrix4x4 MakeRotateYMatrix(float angle);
Matrix4x4 MakeRotateZMatrix(float angle);
Matrix4x4 MakeTranslateMatrix(const Vector3& translation);

Vector3 TransformNormal(const Vector3& vector, const Matrix4x4& matrix);
Matrix4x4 MakeUVTransformMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);