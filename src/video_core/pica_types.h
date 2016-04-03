// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cmath>
#include <cstring>

#include "common/common_types.h"

namespace Pica {

/**
 * Template class for converting arbitrary Pica float types to IEEE 754 32-bit single-precision
 * floating point.
 *
 * When decoding, format is as follows:
 *  - The first `M` bits are the mantissa
 *  - The next `E` bits are the exponent
 *  - The last bit is the sign bit
 *
 * @todo Verify on HW if this conversion is sufficiently accurate.
 */
template<unsigned M, unsigned E>
struct Float {
public:
    static Float<M, E> FromFloat32(float val) {
        Float<M, E> ret;
        ret.value = val;
        return ret;
    }

    static Float<M, E> FromRaw(u32 hex) {
        Float<M, E> res;

        const int width = M + E + 1;
        const int bias = 128 - (1 << (E - 1));
        const int exponent = (hex >> M) & ((1 << E) - 1);
        const unsigned mantissa = hex & ((1 << M) - 1);

        if (hex & ((1 << (width - 1)) - 1))
            hex = ((hex >> (E + M)) << 31) | (mantissa << (23 - M)) | ((exponent + bias) << 23);
        else
            hex = ((hex >> (E + M)) << 31);

        std::memcpy(&res.value, &hex, sizeof(float));

        return res;
    }

    static Float<M, E> Zero() {
        return FromFloat32(0.f);
    }

    // Not recommended for anything but logging
    float ToFloat32() const {
        return value;
    }

    Float<M, E> operator * (const Float<M, E>& flt) const {
        if ((this->value == 0.f && !std::isnan(flt.value)) ||
            (flt.value == 0.f && !std::isnan(this->value)))
            // PICA gives 0 instead of NaN when multiplying by inf
            return Zero();
        return Float<M, E>::FromFloat32(ToFloat32() * flt.ToFloat32());
    }

    Float<M, E> operator / (const Float<M, E>& flt) const {
        return Float<M, E>::FromFloat32(ToFloat32() / flt.ToFloat32());
    }

    Float<M, E> operator + (const Float<M, E>& flt) const {
        return Float<M, E>::FromFloat32(ToFloat32() + flt.ToFloat32());
    }

    Float<M, E> operator - (const Float<M, E>& flt) const {
        return Float<M, E>::FromFloat32(ToFloat32() - flt.ToFloat32());
    }

    Float<M, E>& operator *= (const Float<M, E>& flt) {
        if ((this->value == 0.f && !std::isnan(flt.value)) ||
            (flt.value == 0.f && !std::isnan(this->value)))
            // PICA gives 0 instead of NaN when multiplying by inf
            *this = Zero();
        else value *= flt.ToFloat32();
        return *this;
    }

    Float<M, E>& operator /= (const Float<M, E>& flt) {
        value /= flt.ToFloat32();
        return *this;
    }

    Float<M, E>& operator += (const Float<M, E>& flt) {
        value += flt.ToFloat32();
        return *this;
    }

    Float<M, E>& operator -= (const Float<M, E>& flt) {
        value -= flt.ToFloat32();
        return *this;
    }

    Float<M, E> operator - () const {
        return Float<M, E>::FromFloat32(-ToFloat32());
    }

    bool operator < (const Float<M, E>& flt) const {
        return ToFloat32() < flt.ToFloat32();
    }

    bool operator > (const Float<M, E>& flt) const {
        return ToFloat32() > flt.ToFloat32();
    }

    bool operator >= (const Float<M, E>& flt) const {
        return ToFloat32() >= flt.ToFloat32();
    }

    bool operator <= (const Float<M, E>& flt) const {
        return ToFloat32() <= flt.ToFloat32();
    }

    bool operator == (const Float<M, E>& flt) const {
        return ToFloat32() == flt.ToFloat32();
    }

    bool operator != (const Float<M, E>& flt) const {
        return ToFloat32() != flt.ToFloat32();
    }

private:
    static const unsigned MASK = (1 << (M + E + 1)) - 1;
    static const unsigned MANTISSA_MASK = (1 << M) - 1;
    static const unsigned EXPONENT_MASK = (1 << E) - 1;

    // Stored as a regular float, merely for convenience
    // TODO: Perform proper arithmetic on this!
    float value;
};

using float24 = Float<16, 7>;
using float20 = Float<12, 7>;
using float16 = Float<10, 5>;

/**
 * Template class for handling arbitrary Pica fixed point types
 */
template<unsigned I, unsigned F, typename T>
struct Fixed {

static_assert(std::numeric_limits<T>::is_integer, "T must be an integer type");
using U = typename std::make_unsigned<T>::type;

public:

    static Fixed<I, F, T> FromRaw(T hex) {
        Fixed<I, F, T> ret;
        //TODO: Sign extend if the sign is too short?
        ret.value = hex;
        return ret;
    }

    static Fixed<I, F, T> FromFixed(T i, U f = 0U) {
        return FromRaw(((i << F) & ~FRAC_MASK) | (f & FRAC_MASK));
    }

    static Fixed<I, F, T> FromFloat32(float val) {
        return FromRaw((T)roundf(val * (float)(1 << F)));
    }

    static Fixed<I, F, T> Zero() {
        return FromRaw(0);
    }

    float ToFloat32() const {
        //FIXME: Handle signed case!
        float value = Int() + Fract() / (float)FRAC_MASK;
        return value;
    }

    T ToRaw() const {
        return value;
    }

    U Fract() const {
        return fracpart;
    }

    T Int() const {
        return intpart;
    }

    static U FracMask() {
        return FRAC_MASK;
    }

    static U IntMask() {
        return INT_MASK << F;
    }

    Fixed<I, F, T> operator = (const Fixed<I, F, T>& fxd) const {
        return FromRaw(fxd.value);
    }

    Fixed<I, F, T> operator * (const Fixed<I, F, T>& fxd) const {
        return FromRaw((value * fxd.value) >> F);
    }

    Fixed<I, F, T> operator / (const Fixed<I, F, T>& fxd) const {
        return FromRaw((value << F) / fxd.value);
    }

    Fixed<I, F, T> operator + (const Fixed<I, F, T>& fxd) const {
        return FromRaw(value + fxd.value);
    }

    Fixed<I, F, T> operator - (const Fixed<I, F, T>& fxd) const {
        return FromRaw(value - fxd.value);
    }

    Fixed<I, F, T>& operator *= (const Fixed<I, F, T>& fxd) {
        value = (value * fxd.value) >> F;
        return *this;
    }

    Fixed<I, F, T>& operator /= (Fixed<I, F, T>& fxd) {
        value = (value << F) / fxd.value;
        return *this;
    }

    Fixed<I, F, T>& operator += (Fixed<I, F, T>& fxd) {
        value += fxd.value;
        return *this;
    }

    Fixed<I, F, T>& operator -= (Fixed<I, F, T>& fxd) {
        value -= fxd.value;
        return *this;
    }

    Fixed<I, F, T> operator - () const {
        return FromRaw(-value);
    }

    bool operator < (const Fixed<I, F, T>& fxd) const {
        return value < fxd.value;
    }

    bool operator > (const Fixed<I, F, T>& fxd) const {
        return value > fxd.value;
    }

    bool operator >= (const Fixed<I, F, T>& fxd) const {
        return value >= fxd.value;
    }

    bool operator <= (const Fixed<I, F, T>& fxd) const {
        return value <= fxd.value;
    }

    bool operator == (const Fixed<I, F, T>& fxd) const {
        return value == fxd.value;
    }

    bool operator != (const Fixed<I, F, T>& fxd) const {
        return value != fxd.value;
    }

private:
    static const U MASK = (1 << (I + F)) - 1U;
    static const U INT_MASK = (1 << I) - 1U;
    static const U FRAC_MASK = (1 << F) - 1U;

    union {
        T value;
        BitField<F,I,T> intpart;
        BitField<0,F,U> fracpart;
    };
};

using fixedS28P4 = Fixed<28, 4, s32>;

} // namespace Pica
