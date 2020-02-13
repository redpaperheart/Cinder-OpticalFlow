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

struct Particle
{
	vec3	pos;
	vec3	ppos;
	vec3	initPos;
	float	damping;
	vec4    color;
};

//const int mNumParticles = static_cast<int>(200e3);

class ParticleSystemApp : public App {
  public:
	  void								setup() override;
	  void								update() override;
	  void								draw() override;
	  gl::TextureRef					flipCamTexture();

	  // Optical Flow variables
	  float								mLambda;
	  float								mBlurAmount;
	  float								mDisplacement;
	  CaptureRef						mCapture;
	  gl::TextureRef					mDefaultCamTexture;
	  gl::FboRef						mCamTexFlippedFbo;
	  OpticalFlow						mOpticalFlow;
	  int								mCamResX = 640;
	  int								mCamResY = 480;

	  // Particle system variables 
	  int								mParticleRangeX = mCamResX * 2;
	  int								mParticleRangeY = mCamResY * 2;
	  float								mParticleSize = 5.0;
	  int								mParticlesAlongX = 32;
	  int								mParticlesAlongY = 24;
	  int								mParticleGapX = mParticleRangeX / mParticlesAlongX;
	  int								mParticleGapY = mParticleRangeY / mParticlesAlongY;
	  int								mParticlePosOffsetX = (mParticleRangeX / mParticlesAlongX)/2;
	  int								mParticlePosOffsetY = (mParticleRangeY / mParticlesAlongY)/2;
	  const int							mNumParticles = static_cast<int>(mParticlesAlongX * mParticlesAlongY);
	  float								mForceMult = 10.0;



private:
	enum { WORK_GROUP_SIZE = 128, };
	gl::GlslProgRef						mRenderProg;
	gl::GlslProgRef						mUpdateProg;

	// Descriptions of particle data layout.
	gl::VaoRef		mAttributes[2];
	// Buffers holding raw particle data on GPU.
	gl::VboRef		mParticleBuffer[2];

	// Current source and destination buffers for transform feedback.
	// Source and destination are swapped each frame after update.
	std::uint32_t	mSourceIndex = 0;
	std::uint32_t	mDestinationIndex = 1;

	// Buffers holding raw particle data on GPU.
	//gl::SsboRef							mParticleBuffer;
	//gl::VboRef							mIdsVbo;
	//gl::VaoRef							mAttributes;
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

	gl::Fbo::Format format;
	format.setColorTextureFormat(gl::Texture::Format().internalFormat(GL_RGBA));
	mCamTexFlippedFbo = gl::Fbo::create(mCamResX, mCamResY, format);

	mOpticalFlow.setup(mCamResX, mCamResY);
	mOpticalFlow.setLambda(0.1);
	mOpticalFlow.setBlur(0.5);
	mOpticalFlow.setDisplacement(0.05f);

	// Create initial particle layout.
	vector<Particle> particles;
	particles.assign(mNumParticles, Particle());
	
	for (int i = 0; i < mParticlesAlongX; i++) {
		for (int j = 0; j < mParticlesAlongY; j++) {
			float x = (mParticleGapX * i) + mParticlePosOffsetX; 
			float y = (mParticleGapY * j) + mParticlePosOffsetY; 
			float z = 0.0;
			// CI_LOG_I(mParticlesAlongX * j + i);
			auto& p = particles.at(mParticlesAlongX * j + i);
			p.pos = vec3(x, y, z);
			p.initPos = p.pos;
			p.ppos = p.initPos;
			p.damping = 0.85f;
			p.color = vec4(1.0);//)vec4(c.r, c.g, c.b, 1.0f);
		}
	}
	// Create particle buffers on GPU and copy data into the first buffer.
	// Mark as static since we only write from the CPU once.
	mParticleBuffer[mSourceIndex] = gl::Vbo::create(GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), particles.data(), GL_STATIC_DRAW);
	mParticleBuffer[mDestinationIndex] = gl::Vbo::create(GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), nullptr, GL_STATIC_DRAW);

	for (int i = 0; i < 2; ++i)
	{	// Describe the particle layout for OpenGL.
		mAttributes[i] = gl::Vao::create();
		gl::ScopedVao vao(mAttributes[i]);

		// Define attributes as offsets into the bound particle buffer
		gl::ScopedBuffer buffer(mParticleBuffer[i]);
		gl::enableVertexAttribArray(0);
		gl::enableVertexAttribArray(1);
		gl::enableVertexAttribArray(2);
		gl::enableVertexAttribArray(3);
		gl::enableVertexAttribArray(4);
		gl::vertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, pos));
		gl::vertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, ppos));
		gl::vertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, initPos));
		gl::vertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, damping));
		gl::vertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, color));
	}

	// Load our update program.
	// Match up our attribute locations with the description we gave.

	//mRenderProg = gl::getStockShader(gl::ShaderDef().color());
	try {
		mRenderProg = gl::GlslProg::create(gl::GlslProg::Format().vertex(loadAsset("glsl/particles/particleRender.vert"))
			.fragment(loadAsset("glsl/particles/particleRender.frag"))
			.geometry(loadAsset("glsl/particles/particleRender.geom"))
			.attribLocation("particleId", 0));
	}
	catch (gl::GlslProgCompileExc e) {
		ci::app::console() << e.what() << std::endl;
		quit();
	}
	mUpdateProg = gl::GlslProg::create(gl::GlslProg::Format().vertex(loadAsset("glsl/particles/particleUpdate.vert"))
		.feedbackFormat(GL_INTERLEAVED_ATTRIBS)
		.feedbackVaryings({ "position", "pposition", "initPos", "damping", "color" })
		.attribLocation("iPosition", 0)
		.attribLocation("iPPosition", 1)
		.attribLocation("iInitPos", 2)
		.attribLocation("iDamping", 3)
		.attribLocation("iColor", 4)
	);

	//// Optical Flow setup
	//try {
	//	mCapture = Capture::create(mCamResX, mCamResY);
	//	mCapture->start();
	//	CI_LOG_I("Default camera started");
	//}
	//catch (ci::Exception & exc) {
	//	CI_LOG_EXCEPTION("Failed to init capture ", exc);
	//}

	//gl::Fbo::Format format;
	//format.setColorTextureFormat(gl::Texture::Format().internalFormat(GL_RGBA));
	//mCamTexFlippedFbo = gl::Fbo::create(mCamResX, mCamResY, format);

	//mOpticalFlow.setup(mCamResX, mCamResY);
	//mOpticalFlow.setLambda(0.1);
	//mOpticalFlow.setBlur(0.5);
	//mOpticalFlow.setDisplacement(0.05f);

	//// Particle system setup.
	//vector<Particle> particles;
	//particles.assign(mNumParticles, Particle());


	//for (int i = 0; i < mParticlesAlongX; i++) {
	//	for (int j = 0; j < mParticlesAlongY; j++) {
	//		float x = (mParticleGapX * i) + mParticlePosOffsetX; 
	//		float y = (mParticleGapY * j) + mParticlePosOffsetY; 
	//		float z = 0.0;
	//		// CI_LOG_I(mParticlesAlongX * j + i);
	//		auto& p = particles.at(mParticlesAlongX * j + i);
	//		p.pos = vec3(x, y, z);
	//		p.initPos = p.pos;
	//		p.ppos = p.initPos;
	//		p.damping = 0.85f;
	//		p.color = vec4(1.0);//)vec4(c.r, c.g, c.b, 1.0f);
	//	}
	//}

	//ivec3 count = gl::getMaxComputeWorkGroupCount();
	//CI_ASSERT(count.x >= (mNumParticles / WORK_GROUP_SIZE));

	//// Create particle buffers on GPU and copy data into the first buffer.
	//// Mark as static since we only write from the CPU once.
	//mParticleBuffer = gl::Ssbo::create(particles.size() * sizeof(Particle), particles.data(), GL_STATIC_DRAW);
	//gl::ScopedBuffer scopedParticleSsbo(mParticleBuffer);
	//mParticleBuffer->bindBase(0);

	//// Create a default color shader.
	//try {
	//	mRenderProg = gl::GlslProg::create(gl::GlslProg::Format().vertex(loadAsset("glsl/particles/particleRender.vert"))
	//		.fragment(loadAsset("glsl/particles/particleRender.frag"))
	//		.geometry(loadAsset("glsl/particles/particleRender.geom"))
	//		.attribLocation("particleId", 0));
	//}
	//catch (gl::GlslProgCompileExc e) {
	//	ci::app::console() << e.what() << std::endl;
	//	quit();
	//}

	//std::vector<GLuint> ids(mNumParticles);
	//GLuint currId = 0;
	//std::generate(ids.begin(), ids.end(), [&currId]() -> GLuint { return currId++; });

	//mIdsVbo = gl::Vbo::create<GLuint>(GL_ARRAY_BUFFER, ids, GL_STATIC_DRAW);
	//mAttributes = gl::Vao::create();
	//gl::ScopedVao vao(mAttributes);
	//gl::ScopedBuffer scopedIds(mIdsVbo);
	//gl::enableVertexAttribArray(0);
	//gl::vertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0);

	//try {
	//	//// Load our update program.
	//	mUpdateProg = gl::GlslProg::
	//		create(gl::GlslProg::Format().compute(loadAsset("glsl/particles/particleUpdate.comp")));
	//}
	//catch (gl::GlslProgCompileExc e) {
	//	ci::app::console() << e.what() << std::endl;
	//	quit();
	//}
}

gl::TextureRef ParticleSystemApp::flipCamTexture() {
	gl::clear(Color::black());
	gl::ScopedFramebuffer fbScpLastTex(mCamTexFlippedFbo);
	gl::ScopedViewport scpVpLastTex(ivec2(0), mCamTexFlippedFbo->getSize());
	gl::ScopedMatrices matFlipTex;
	gl::setMatricesWindow(mCamTexFlippedFbo->getSize());
	gl::scale(-1, 1);
	gl::translate(-mCamTexFlippedFbo->getWidth(), 0);
	gl::draw(mDefaultCamTexture, mCamTexFlippedFbo->getBounds());
	return mCamTexFlippedFbo->getColorTexture();
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
			mOpticalFlow.update(flipCamTexture());
		}
	}

	// Update particles on the GPU
	gl::ScopedGlslProg prog(mUpdateProg);
	gl::ScopedState rasterizer(GL_RASTERIZER_DISCARD, true);	// turn off fragment stage
	gl::ScopedTextureBind texScope(mOpticalFlow.getOpticalFlowBlurTexture(), 0);
	mUpdateProg->uniform("uCanvasSize", vec2(mParticleRangeX, mParticleRangeY));
	mUpdateProg->uniform("uForceMult", mForceMult);
	// Bind the source data (Attributes refer to specific buffers).
	gl::ScopedVao source(mAttributes[mSourceIndex]);
	// Bind destination as buffer base.
	gl::bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mParticleBuffer[mDestinationIndex]);
	gl::beginTransformFeedback(GL_POINTS);

	// Draw source into destination, performing our vertex transformations.
	gl::drawArrays(GL_POINTS, 0, mNumParticles);

	gl::endTransformFeedback();

	// Swap source and destination for next loop
	std::swap(mSourceIndex, mDestinationIndex);

	//gl::ScopedGlslProg prog(mUpdateProg);
	//gl::ScopedTextureBind texScope(mOpticalFlow.getOpticalFlowBlurTexture(), 0);
	//mUpdateProg->uniform("uCanvasSize", vec2(mParticleRangeX, mParticleRangeY));
	//mUpdateProg->uniform("uForceMult", mForceMult);
	//gl::ScopedBuffer scopedParticleSsbo(mParticleBuffer);
	//gl::dispatchCompute(mNumParticles / WORK_GROUP_SIZE, 1, 1);
	//gl::memoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void ParticleSystemApp::draw(){
	gl::clear(Color(0, 0, 0));

	// draw camera feed
	{
		//gl::ScopedMatrices matOpticalFlow;
		//gl::scale(mParticleRangeX / mCamResX, mParticleRangeY / mCamResY);
		//mOpticalFlow.drawFlowGrid();

		gl::ScopedMatrices matCamFeed;
		Rectf bounds(0, 0, mParticleRangeX, mParticleRangeY);
		gl::draw(flipCamTexture(), bounds);
	
	}

	// darw particle system on top of camera feed
	{
		//gl::setMatricesWindowPersp(getWindowSize(), 60.0f, 1.0f, 10000.0f);
		//gl::enableDepthRead();
		//gl::enableDepthWrite();

		gl::ScopedGlslProg render(mRenderProg);
		gl::ScopedVao vao(mAttributes[mSourceIndex]);
		glEnable(GL_PROGRAM_POINT_SIZE);
		mRenderProg->uniform("uParticleSize", mParticleSize);
		gl::context()->setDefaultShaderVars();
		gl::drawArrays(GL_POINTS, 0, mNumParticles);
		
		//gl::ScopedGlslProg render(mRenderProg);
		//gl::ScopedBuffer scopedParticleSsbo(mParticleBuffer);
		//gl::ScopedVao vao(mAttributes);
		//glEnable(GL_PROGRAM_POINT_SIZE);
		//mRenderProg->uniform("uParticleSize", mParticleSize);
		//gl::context()->setDefaultShaderVars();
		//gl::drawArrays(GL_POINTS, 0, mNumParticles);
	}

	// draw optical flow at top right corner of app window
	{
		gl::ScopedMatrices matOpticalFlow;
		gl::translate(appScreenWidth - mOpticalFlow.getWidth(), 0);
		mOpticalFlow.drawFlowGrid();
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
