#pragma once


class TestScene
{
public:
	virtual ~TestScene() {}
	virtual void Update(double frameTime, float frameDeltaTime) = 0;
	virtual void Draw() = 0;
};
