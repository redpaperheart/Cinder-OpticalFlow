/*

	ParticleSystemApp.h
		- Shuvashis Das | Red Paper Heart Studio

*/

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"
#include "cinder/Utilities.h"

#include "cinder/Capture.h"
#include "OpticalFlow.h"

using namespace ci;
using namespace ci::app;
using namespace std;

int								appScreenWidth = 1920;
int								appScreenHeight = 1080;

#pragma pack( push, 1 )
struct Particle
{
	vec3	pos;
	float   pad1;
	vec3	ppos;
	float   pad2;
	vec3	home;
	float   pad3;
	vec4    color;
	float	damping;
	vec3    pad4;
};
#pragma pack( pop )

//const int mNumParticles = static_cast<int>(200e3);

class ParticleSystemApp : public App {
  public:
	  void								setup() override;
	  void								update() override;
	  void								draw() override;

	  // Optical Flow variables
	  float								mLambda;
	  float								mBlurAmount;
	  float								mDisplacement;
	  CaptureRef						mCapture;
	  gl::TextureRef					mDefaultCamTexture;
	  OpticalFlow						mOpticalFlow;
	  int								mCamResX = 640;
	  int								mCamResY = 480;

	  // Particle system variables 
	  int								mParticleRangeX = mCamResX * 2;
	  int								mParticleRangeY = mCamResY * 2;
	  float								mPointSize = 10.0;
	  int								mParticlesAlongX = 16;
	  int								mParticlesAlongY = 12;
	  int								mParticleGapX = mParticleRangeX / mParticlesAlongX;
	  int								mParticleGapY = mParticleRangeY / mParticlesAlongY;
	  int								mParticlePosOffsetX = (mParticleRangeX / mParticlesAlongX)/2;
	  int								mParticlePosOffsetY = (mParticleRangeY / mParticlesAlongY)/2;
	  const int							mNumParticles = static_cast<int>(mParticlesAlongX * mParticlesAlongY);
	  float								mForceMult = 2.0;



private:
	enum { WORK_GROUP_SIZE = 128, };
	gl::GlslProgRef						mRenderProg;
	gl::GlslProgRef						mUpdateProg;

	// Buffers holding raw particle data on GPU.
	gl::SsboRef							mParticleBuffer;
	gl::VboRef							mIdsVbo;
	gl::VaoRef							mAttributes;
};

void ParticleSystemApp::setup(){
	gl::enableAlphaBlending();


	// Optical Flow setup
	try {
		mCapture = Capture::create(mCamResX, mCamResY);
		mCapture->start();
		CI_LOG_I("Default camera started");
	}
	catch (ci::Exception & exc) {
		CI_LOG_EXCEPTION("Failed to init capture ", exc);
	}

	mOpticalFlow.setup(mCamResX, mCamResY);
	mOpticalFlow.setLambda(0.1);
	mOpticalFlow.setBlur(0.5);
	mOpticalFlow.setDisplacement(0.05f);



	// Particle system setup.
	vector<Particle> particles;
	particles.assign(mNumParticles, Particle());


	for (int i = 0; i < mParticlesAlongX; i++) {
		for (int j = 0; j < mParticlesAlongY; j++) {
			float x = (mParticleGapX * i) + mParticlePosOffsetX; // Rand::randFloat(0, mParticleRangeX);
			float y = (mParticleGapY * j) + mParticlePosOffsetY; //Rand::randFloat(0, mParticleRangeY);
			float z = 0.0;
			CI_LOG_I(mParticlesAlongX * j + i);
			auto& p = particles.at(mParticlesAlongX * j + i);
			p.pos = vec3(x, y, z);
			p.home = p.pos;
			p.ppos = p.home + Rand::randVec3();// *5.0f;
			p.damping = Rand::randFloat(0.9f, 0.985f);
			p.color = vec4(1.0);//)vec4(c.r, c.g, c.b, 1.0f);
		}
	}

	ivec3 count = gl::getMaxComputeWorkGroupCount();
	CI_ASSERT(count.x >= (mNumParticles / WORK_GROUP_SIZE));

	// Create particle buffers on GPU and copy data into the first buffer.
	// Mark as static since we only write from the CPU once.
	mParticleBuffer = gl::Ssbo::create(particles.size() * sizeof(Particle), particles.data(), GL_STATIC_DRAW);
	gl::ScopedBuffer scopedParticleSsbo(mParticleBuffer);
	mParticleBuffer->bindBase(0);

	// Create a default color shader.
	try {
		mRenderProg = gl::GlslProg::create(gl::GlslProg::Format().vertex(loadAsset("glsl/particles/particleRender.vert"))
			.fragment(loadAsset("glsl/particles/particleRender.frag"))
			.attribLocation("particleId", 0));
	}
	catch (gl::GlslProgCompileExc e) {
		ci::app::console() << e.what() << std::endl;
		quit();
	}

	std::vector<GLuint> ids(mNumParticles);
	GLuint currId = 0;
	std::generate(ids.begin(), ids.end(), [&currId]() -> GLuint { return currId++; });

	mIdsVbo = gl::Vbo::create<GLuint>(GL_ARRAY_BUFFER, ids, GL_STATIC_DRAW);
	mAttributes = gl::Vao::create();
	gl::ScopedVao vao(mAttributes);
	gl::ScopedBuffer scopedIds(mIdsVbo);
	gl::enableVertexAttribArray(0);
	gl::vertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0);

	try {
		//// Load our update program.
		mUpdateProg = gl::GlslProg::
			create(gl::GlslProg::Format().compute(loadAsset("glsl/particles/particleUpdate.comp")));
	}
	catch (gl::GlslProgCompileExc e) {
		ci::app::console() << e.what() << std::endl;
		quit();
	}
}

void ParticleSystemApp::update(){
	//optical flow update
	if (mCapture && mCapture->checkNewFrame()) {
		if (!mDefaultCamTexture) {
			// Capture images come back as top-down, and it's more efficient to keep them that way
			mDefaultCamTexture = gl::Texture::create(*mCapture->getSurface(), gl::Texture::Format());
		}
		else {
			mDefaultCamTexture->update(*mCapture->getSurface());
			mOpticalFlow.update(mDefaultCamTexture);
		}
	}

	// Update particles on the GPU
	gl::ScopedGlslProg prog(mUpdateProg);

	gl::ScopedTextureBind texScope(mOpticalFlow.getOpticalFlowBlurTexture(), 0);
	mUpdateProg->uniform("uCanvasSize", vec2(mParticleRangeX, mParticleRangeY));
	mUpdateProg->uniform("uForceMult", mForceMult);

	gl::ScopedBuffer scopedParticleSsbo(mParticleBuffer);

	gl::dispatchCompute(mNumParticles / WORK_GROUP_SIZE, 1, 1);
	gl::memoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void ParticleSystemApp::draw(){
	gl::clear(Color(0, 0, 0));

	// draw optical flow at top right corner of app window
	{
		gl::ScopedMatrices matOpticalFlow;
		gl::translate(appScreenWidth - mOpticalFlow.getWidth(), 0);
		mOpticalFlow.drawFlowGrid();
	}

	// draw camera feed
	{
		Rectf bounds(0, 0, mParticleRangeX, mParticleRangeY);
		gl::draw(mDefaultCamTexture, bounds);
	}

	// darw particle system on top of camera feed
	{
		gl::ScopedGlslProg render(mRenderProg);
		gl::ScopedBuffer scopedParticleSsbo(mParticleBuffer);
		gl::ScopedVao vao(mAttributes);
		glEnable(GL_PROGRAM_POINT_SIZE);
		mRenderProg->uniform("uPointSize", mPointSize);
		gl::context()->setDefaultShaderVars();
		gl::drawArrays(GL_POINTS, 0, mNumParticles);
	}

	// log FPS on screen
	{
		gl::drawString(toString(static_cast<int>(getAverageFps())) + " fps", vec2(appScreenWidth - 100, appScreenHeight - 100));
	}
}

CINDER_APP(ParticleSystemApp, RendererGl, [](App::Settings* settings) {
	settings->setWindowSize(appScreenWidth, appScreenHeight);
	settings->setMultiTouchEnabled(false);
})
