#pragma once
#include <array>
#include <cmath>
#include <cassert>
#include <iostream>

// Vector
template<int n> struct vec {
    double data[n] = {0};
    double& operator[](const int i)       { assert(i>=0 && i<n); return data[i]; }
    double  operator[](const int i) const { assert(i>=0 && i<n); return data[i]; }

    friend vec<n> operator+(const vec<n>& v1, const vec<n>& v2) {
        vec<n> sum;
        for (int i = 0; i < n; ++i) {
            sum.data[i] = v1.data[i] + v2.data[i];
        }
        return sum;
    }

    friend vec<n> operator-(const vec<n>& v1, const vec<n>& v2) {
        vec<n> diff;
        for (int i = 0; i < n; ++i) {
            diff.data[i] = v1.data[i] - v2.data[i];
        }
        return diff;
    }

    double dotProduct(const vec<n>& v1) const {
        double product = 0;
        for (int i = 0; i < n; ++i) {
            product += data[i] * v1.data[i];
        }
        return product;
    }
};

template<int n> std::ostream& operator<<(std::ostream& out, const vec<n>& v) {
    for (int i=0; i<n; i++) out << v[i] << " ";
    return out;
}

template<> struct vec<2> {
    double x = 0, y = 0;
    double& operator[](const int i)       { assert(i>=0 && i<2); return i ? y : x; }
    double  operator[](const int i) const { assert(i>=0 && i<2); return i ? y : x; }
};

template<> struct vec<3> {
    double x = 0, y = 0, z = 0;
    double& operator[](const int i)       { assert(i>=0 && i<3); return i ? (1==i ? y : z) : x; }
    double  operator[](const int i) const { assert(i>=0 && i<3); return i ? (1==i ? y : z) : x; }
};

template<> struct vec<4> {
    double x = 0, y = 0, z = 0, w = 0;
    double& operator[](const int i)       { assert(i>=0 && i<4); return i ? (1==i ? y : (2==i ? z : w )) : x; }
    double  operator[](const int i) const { assert(i>=0 && i<4); return i ? (1==i ? y : (2==i ? z : w )) : x; }
};

typedef vec<2> vec2;
typedef vec<3> vec3;
typedef vec<4> vec4;

// Matrix
template <int R, int C, typename T = double>
struct mat {
    std::array<T, R * C> data{};

    T& operator[] (int r, int c) {
        return data[r * C + c];
    }

    const T& operator[] (int r, int c) const {
        return data[r * C + c];
    }

    friend mat<R, C> operator+(const mat<R, C>& m1, const mat<R, C>& m2) {
        mat<R, C> sum;
        for (int i = 0; i < R; ++i) 
            for (int j = 0; j < C; ++j) {
                sum.data[i * C + j] = m1.data[i * C + j] + m2.data[i * C + j];
        }
        return sum;
    }

    friend mat<R, C> operator-(const mat<R, C>& m1, const mat<R, C>& m2) {
        mat<R, C> diff;
        for (int i = 0; i < R; ++i) 
            for (int j = 0; j < C; ++j) {
                diff.data[i * C + j] = m1.data[i * C + j] - m2.data[i * C + j];
        }
        return diff;
    }

    mat<C, R, T> transpose() const {
        mat<C, R, T> res;
        for (int i = 0; i < R; ++i)
            for (int j = 0; j < C; ++j) {
                res.data[j * R + i] = data[i * C + j]; 
        }
        return res;
    }
};

template <int R, int M, int C, typename T = double>
mat<R, C, T> operator*(const mat<R, M, T>& m1, const mat<M, C, T>& m2) {
    mat<R, C, T> prod;
    for (int i = 0; i < R; ++i) 
        for (int j = 0; j < C; ++j) 
            for (int k = 0; k < M; ++k) {
                prod.data[i * C + j] += m1.data[i * M + k] * m2.data[k * C + j];
    }
    return prod;
}


template <typename T>
mat<2, 2, T> inverse(const mat<2, 2, T>& m) {
    T det = m.data[0] * m.data[3] - m.data[1] * m.data[2];
    assert(std::abs(det) > 1e-10 && "Matrix is singular, cannot invert");
    T inv_det = T(1) / det;
    mat<2, 2, T> res;
    res.data[0] =  m.data[3] * inv_det;
    res.data[1] = -m.data[1] * inv_det;
    res.data[2] = -m.data[2] * inv_det;
    res.data[3] =  m.data[0] * inv_det;
    return res;
}


template <typename T>
mat<3, 3, T> inverse(const mat<3, 3, T>& m) {
    // Cofactors
    T c00 = m.data[4]*m.data[8] - m.data[5]*m.data[7];
    T c01 = m.data[5]*m.data[6] - m.data[3]*m.data[8];
    T c02 = m.data[3]*m.data[7] - m.data[4]*m.data[6];
    T c10 = m.data[2]*m.data[7] - m.data[1]*m.data[8];
    T c11 = m.data[0]*m.data[8] - m.data[2]*m.data[6];
    T c12 = m.data[1]*m.data[6] - m.data[0]*m.data[7];
    T c20 = m.data[1]*m.data[5] - m.data[2]*m.data[4];
    T c21 = m.data[2]*m.data[3] - m.data[0]*m.data[5];
    T c22 = m.data[0]*m.data[4] - m.data[1]*m.data[3];

    T det = m.data[0]*c00 + m.data[1]*c01 + m.data[2]*c02;
    assert(std::abs(det) > 1e-10 && "Matrix is singular, cannot invert");
    T inv_det = T(1) / det;

    mat<3, 3, T> res;
    res.data[0] = c00 * inv_det;  res.data[1] = c10 * inv_det;  res.data[2] = c20 * inv_det;
    res.data[3] = c01 * inv_det;  res.data[4] = c11 * inv_det;  res.data[5] = c21 * inv_det;
    res.data[6] = c02 * inv_det;  res.data[7] = c12 * inv_det;  res.data[8] = c22 * inv_det;
    return res;
}

