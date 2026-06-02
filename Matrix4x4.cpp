#include "Matrix4x4.h"
#include <cmath>
#include <cstdint>
#include <numbers>
#define _USE_MATH_DEFINES

Matrix4x4 Inverse(const Matrix4x4& m)
{
	Matrix4x4 result = {};
	float a[4][8] = { 0 };

	// 拡大係数行列の作成（左側に元の行列、右側に単位行列）
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			a[i][j] = m.m[i][j];
		}
		a[i][4 + i] = 1.0f;
	}

	// ガウス・ジョルダンの消去法（掃き出し法）
	for (int i = 0; i < 4; ++i) {
		float pivot = a[i][i];
		if (pivot == 0.0f) continue; // ゼロ除算回避（本来は行の入れ替えが必要ですが簡易化）

		// ピボット行をピボットで割る
		for (int j = 0; j < 8; ++j) {
			a[i][j] /= pivot;
		}

		// ピボット列の他の行を0にする
		for (int k = 0; k < 4; ++k) {
			if (i != k) {
				float factor = a[k][i];
				for (int j = 0; j < 8; ++j) {
					a[k][j] -= factor * a[i][j];
				}
			}
		}
	}

	// 右側の単位行列だった部分が逆行列になる
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			result.m[i][j] = a[i][4 + j];
		}
	}

	return result;
}

Matrix4x4 MakeIdentity4x4()
{
	Matrix4x4 result = {};
	result.m[0][0] = 1.0f;
	result.m[1][1] = 1.0f;
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) {
	Matrix4x4 r = {};
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k) {
				sum += a.m[i][k] * b.m[k][j];
			}
			r.m[i][j] = sum;
		}
	}
	return r;
}

// ベクトルを4x4行列で変換する関数
Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix)
{
	Vector3 result;

	// 平行移動（m[3][*]）を足し合わせ、最後にwで割る
	result.x = (vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + 1.0f * matrix.m[3][0]);
	result.y = (vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + 1.0f * matrix.m[3][1]);
	result.z = (vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + 1.0f * matrix.m[3][2]);
	// w成分の計算（透視投影の除算に必要）
	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + 1.0f * matrix.m[3][3];

	if (w != 0.0f)
	{
		result.x /= w;
		result.y /= w;
		result.z /= w;
	}
	return result;
}

Matrix4x4 identity() {
	Matrix4x4 result = {};
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; j++)
		{
			result.m[i][j] = (i == j) ? 1.0f : 0.0f;
		}
	}
	return result;
}

Matrix4x4 rotationX(float angle) {
	Matrix4x4 result = identity();
	result.m[0][0] = 1.0f;
	result.m[1][1] = cosf(angle);
	result.m[1][2] = sinf(angle);
	result.m[2][1] = -sinf(angle);
	result.m[2][2] = cosf(angle);
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 rotationY(float angle) {
	Matrix4x4 result = identity();
	result.m[0][0] = cosf(angle);
	result.m[0][2] = -sinf(angle);
	result.m[1][1] = 1.0f;
	result.m[2][0] = sinf(angle);
	result.m[2][2] = cosf(angle);
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 rotationZ(float angle) {
	Matrix4x4 result = identity();
	result.m[0][0] = cosf(angle);
	result.m[0][1] = sinf(angle);
	result.m[1][0] = -sinf(angle);
	result.m[1][1] = cosf(angle);
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 MakeTranslationMatrix(const Vector3& translation) {
	Matrix4x4 result = identity();
	result.m[3][0] = translation.x;
	result.m[3][1] = translation.y;
	result.m[3][2] = translation.z;
	return result;
}

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translation) {
	Matrix4x4 result;
	// スケーリング行列の作成
	Matrix4x4 scaleMatrix = { {
		{scale.x, 0.0f, 0.0f, 0.0f},
		{0.0f, scale.y, 0.0f, 0.0f},
		{0.0f, 0.0f, scale.z, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f}
		} };
	// 回転行列の作成
	Matrix4x4 rotationXMatrix = rotationX(rotate.x);
	Matrix4x4 rotationYMatrix = rotationY(rotate.y);
	Matrix4x4 rotationZMatrix = rotationZ(rotate.z);
	Matrix4x4 rotationMatrix = Multiply(rotationXMatrix, Multiply(rotationYMatrix, rotationZMatrix));
	// 平行移動行列の作成
	Matrix4x4 translationMatrix = MakeTranslationMatrix(translation);
	// アフィン変換行列の計算
	result = Multiply(scaleMatrix, Multiply(rotationMatrix, translationMatrix));
	return result;
}

// 透視投影行列の作成
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspect, float nearClip, float farClip) {
	Matrix4x4 result = {};
	float f = 1.0f / tanf(fovY / 2.0f);
	result.m[0][0] = f / aspect;
	result.m[1][1] = f;
	result.m[2][2] = farClip / (farClip - nearClip);
	result.m[2][3] = 1.0f;
	result.m[3][2] = -nearClip * farClip / (farClip - nearClip);
	return result;
}

// 正射影行列の作成
Matrix4x4 MakeOrthographicMatrix(float left, float right, float top, float bottom, float nearClip, float farClip) {
	Matrix4x4 result = {};
	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[3][0] = -(right + left) / (right - left);
	result.m[3][1] = -(top + bottom) / (top - bottom);
	result.m[3][2] = -nearClip / (farClip - nearClip);
	result.m[3][3] = 1.0f;
	return result;
}

// ビューポート変換行列の作成
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
	Matrix4x4 result = {};
	result.m[0][0] = width / 2.0f;
	result.m[1][1] = -height / 2.0f; // Y軸を反転
	result.m[2][2] = maxDepth - minDepth;
	result.m[3][0] = left + width / 2.0f;
	result.m[3][1] = top + height / 2.0f;
	result.m[3][2] = minDepth;
	result.m[3][3] = 1.0f;
	return result;
}