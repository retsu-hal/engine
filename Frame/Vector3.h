#pragma once
#include "main.h"



//=========================================================================================================
// Vector3クラス ---簡単な3Dベクトルクラス
//=========================================================================================================
class Vector3
{
public:
	float x, y, z;
	Vector3() {}
	Vector3(const Vector3& a) : x(a.x), y(a.y), z(a.z) {}					//コピーコンストラクタ
	Vector3(float nx, float ny, float nz) : x(nx), y(ny), z(nz) {}		//3つの値で作成するコンストラクタ
	
	//標準的なオブジェクトの保守
	//代入　（Cの慣習に従い値への参照を返す）
	Vector3& operator=(const Vector3& a)
	{
		x = a.x; y = a.y; z = a.z;
		return *this; 
	}

	//等しさのチェック
	bool operator==(const Vector3& a) const
	{
		return x == a.x && y == a.y && z == a.z;
	}

	bool operator!=(const Vector3& a) const
	{
		return x != a.x || y != a.y || z != a.z;
	}

//==================
//ベクトル操作
//==================

	//０に設定
	void zero() { x = y = z = 0.0f; }

	//単項式のマイナスは、反転したベクトルを返す。
	Vector3 operator-() const
	{
		return Vector3(-x, -y, -z);
	}

	//加算
	Vector3 operator+(const Vector3& a) const
	{
		return Vector3(x + a.x, y + a.y, z + a.z);
	}

	//減算
	Vector3 operator-(const Vector3& a) const
	{
		return Vector3(x - a.x, y - a.y, z - a.z);
	}

	//乗算
	Vector3 operator*(float a) const
	{
		return Vector3(x * a, y * a, z * a);
	}

	//除算
	Vector3 operator/(float a) const
	{
		float oneOverA = 1.0f / a;
		return Vector3(x * oneOverA, y * oneOverA, z * oneOverA);
	}

//==================
//Cの表記法に準拠するための組み合わせ代入演算
//==================
	Vector3 &operator+=(const Vector3& a)
	{
		x += a.x; y += a.y; z += a.z;
		return *this;
	}

	Vector3 &operator-=(const Vector3& a)
	{
		x -= a.x; y -= a.y; z -= a.z;
		return *this;
	}

	Vector3 &operator*=(float a)
	{
		x *= a; y *= a; z *= a;
		return *this;
	}	

	Vector3 &operator/=(float a)
	{
		float oneOverA = 1.0f / a;
		x *= oneOverA; y *= oneOverA; z *= oneOverA;
		return *this;
	}

//==================
//正規化
//==================
	void normalize()
	{
		float magSq = x * x + y * y + z * z;
		if (magSq > 0.0f)
		{
			float oneOverMag = 1.0f / sqrtf(magSq);
			x *= oneOverMag; 
			y *= oneOverMag;
			z *= oneOverMag;
		}
	}

	//ベクトルの内積
	//標準の乗算記号をこれにオーバーロードする
	float operator*(const Vector3& a) const
	{
		return x * a.x + y * a.y + z * a.z;
	}

	//ベクトルの外積
	static Vector3 cross(const Vector3& a, const Vector3& b)
	{
		return Vector3(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x);
	}

	float length() const
	{
		return sqrtf(x * x + y * y + z * z);
	}

	static float dot(const Vector3& a, const Vector3& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}
};



//=========================================================================================================
// 非メンバー関数
//=========================================================================================================


//ベクトルの大きさ
inline float vectorMag(const Vector3& a)
{
	return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

//ベクトルの外積
inline Vector3 crossProduct(const Vector3& a, const Vector3& b)
{
	return Vector3(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x);
}

//対称性のために、左から乗算する
inline Vector3 operator*(float k, const Vector3& v)
{
	return Vector3(k*v.x, k*v.y, k*v.z);
}


//ベクトルの距離
inline float distance(const Vector3& a, const Vector3& b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	float dz = a.z - b.z;
	return sqrtf(dx * dx + dy * dy + dz * dz);
}


//=========================================================================================================
// グローバル変数
//=========================================================================================================
extern const Vector3 kXeroVector;