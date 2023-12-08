#pragma once

struct ViewMatrix {
	ViewMatrix() noexcept
		: data(){}

	float* operator[](int index) noexcept {
		return data[index];
	}
	const float* operator[](int index) const noexcept {
		return data[index];
	}

	float data[4][4];
};