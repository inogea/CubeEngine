#pragma once


class Job
{
public:
	Job(const class Step* pStep, const class Drawable* pDrawable);
	void Execute(class Graphics& gfx) const noexcept;
private:
	const class Drawable* pDrawable;
	const class Step* pStep;
};