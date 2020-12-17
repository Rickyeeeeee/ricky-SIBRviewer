/*
 * Copyright (C) 2020, Inria
 * GRAPHDECO research group, https://team.inria.fr/graphdeco
 * All rights reserved.
 *
 * This software is free for non-commercial, research and evaluation use 
 * under the terms of the LICENSE.md file.
 *
 * For inquiries contact sibr@inria.fr and/or George.Drettakis@inria.fr
 */


#include <fstream>
#include <core/graphics/Window.hpp>
#include <core/view/MultiViewManager.hpp>
#include <projects/ulr/renderer/TexturedMeshView.hpp>
#include <core/scene/BasicIBRScene.hpp>
#include <core/raycaster/Raycaster.hpp>
#include <core/view/SceneDebugView.hpp>

#define PROGRAM_NAME "sibr_texturedMesh_app"
using namespace sibr;

const char* usage = ""
	"Usage: " PROGRAM_NAME " -path <dataset-path or mesh-path>"    	                                "\n"
	;


struct TexturedMeshAppArgs :
	virtual BasicIBRAppArgs {
	Arg<std::string> textureImagePath = { "texture", "" ,"texture path"};
	Arg<std::string> meshPath = { "mesh", "", "mesh path" };
	Arg<bool> noScene = { "noScene" };
};

int main( int ac, char** av )
{
	{

		// Parse Commad-line Args
		CommandLineArgs::parseMainArgs(ac, av);
		TexturedMeshAppArgs myArgs;

		const bool doVSync = !myArgs.vsync;
		// rendering size
		uint rendering_width = myArgs.rendering_size.get()[0];
		uint rendering_height = myArgs.rendering_size.get()[1];
		// window size
		uint win_width = myArgs.win_width;
		uint win_height = myArgs.win_height;

		// Window setup
		sibr::Window        window(PROGRAM_NAME, sibr::Vector2i(50, 50), myArgs, getResourcesDirectory() + "/ulr/" + PROGRAM_NAME + ".ini");

		// Setup IBR
		BasicIBRScene::Ptr		scene;
		std::string texImgPath, meshPath, m1, m2, m3, t1, t2;

		if (myArgs.noScene) {
			scene = BasicIBRScene::Ptr(new BasicIBRScene());

			if (myArgs.textureImagePath.get() != "") {
				meshPath = myArgs.dataset_path.get();
				texImgPath = myArgs.textureImagePath;
			}
		}
		else {
			// Specify scene initlaization options

/*
			BasicIBRScene::SceneOptions initOpts;
			initOpts.cameras = false;
			initOpts.images = false;
			initOpts.mesh = false;
			initOpts.renderTargets = false;
*/

			scene = BasicIBRScene::Ptr(new BasicIBRScene(myArgs));

			// Cleanup; move the Scene ?
			std::cerr << "Reading " << myArgs.dataset_path.get() + "/capreal" << std::endl;
			if (!directoryExists(myArgs.dataset_path.get() + "/capreal")) {
				std::cerr << "Reading " << parentDirectory(myArgs.dataset_path.get()) + "/capreal" << std::endl;
				if (!directoryExists(parentDirectory(myArgs.dataset_path.get()) + "/capreal"))
					SIBR_ERR << "Cant find capreal and textured mesh" << std::endl;
				else {
					m1 = meshPath = parentDirectory(myArgs.dataset_path.get()) + "/capreal/textured.obj";
					t1 = texImgPath = parentDirectory(myArgs.dataset_path.get()) + "/capreal/textured_u1_v1.png";
					if (!fileExists(meshPath)) {
						m2 = meshPath = parentDirectory(myArgs.dataset_path.get()) + "/capreal/mesh.obj";
						if (!fileExists(texImgPath)) {
							texImgPath = parentDirectory(myArgs.dataset_path.get()) + "/capreal/texture.png";
							if (!fileExists(texImgPath))
								SIBR_ERR << "Cant find texture " << t1 << std::endl;
							else {
								m3 = meshPath = parentDirectory(myArgs.dataset_path.get()) + "/capreal/mesh.ply";
								t2 = texImgPath = parentDirectory(myArgs.dataset_path.get()) + "/capreal/texture.png";
								if (!fileExists(meshPath))
									SIBR_ERR << "Cant find mesh, tried " << m1 << " / " << m2 << " / " << m3 << std::endl;
								if (!fileExists(texImgPath))
									SIBR_ERR << "Cant find texture, tried " << t1 << " / " << t2 << std::endl;
							}
						}
					}
					else if (!fileExists(texImgPath)) {
						t2 = texImgPath = parentDirectory(myArgs.dataset_path.get()) + "/capreal/texture.png";
						if (!fileExists(texImgPath))
							SIBR_ERR << "Cant find texture, tried " << t1 << " / " << t2 << std::endl;
					}
				}
			}
			else {
				meshPath = myArgs.dataset_path.get() + "/capreal/mesh.ply";
				texImgPath = myArgs.dataset_path.get() + "/capreal/texture.png";
			}
		}

		// Load the texture image and provide it to the scene
		sibr::ImageRGB inputTextureImg;
		if (sibr::fileExists(texImgPath)) {
			inputTextureImg.load(texImgPath);
			scene->inputMeshTextures().reset(new sibr::Texture2DRGB(inputTextureImg, SIBR_GPU_LINEAR_SAMPLING));
		}
		else {
			SIBR_ERR << "No mesh and texture found! Please specify path to mesh using --path and path to the mesh texture using --texture!" << std::endl;
			return 0;
		}

		if (myArgs.noScene) {
			Mesh::Ptr newMesh(new Mesh(true));
			newMesh->load(meshPath);
			scene->proxies()->replaceProxyPtr(newMesh);
		}

		// check rendering size; if no rendering-size specified, use 1080p
		rendering_width = (rendering_width <= 0) ? 1920 : rendering_width;
		rendering_height = (rendering_height <= 0) ? 1080 : rendering_height;
		Vector2u usedResolution(rendering_width, rendering_height);

		const unsigned int sceneResWidth = usedResolution.x();
		const unsigned int sceneResHeight = usedResolution.y();


		TexturedMeshView::Ptr	texturedView(new TexturedMeshView(scene, sceneResWidth, sceneResHeight));


		// Raycaster.
		std::shared_ptr<sibr::Raycaster> raycaster = std::make_shared<sibr::Raycaster>();
		raycaster->init();
		raycaster->addMesh(scene->proxies()->proxy());

		// Camera handler for main view.
		sibr::InteractiveCameraHandler::Ptr generalCamera(new InteractiveCameraHandler());
		if( scene->cameras()->inputCameras().size() == 0 ) 
			generalCamera->setup(scene->proxies()->proxyPtr(), Viewport(0, 0, (float)usedResolution.x(), (float)usedResolution.y()));
		else
			generalCamera->setup(scene->cameras()->inputCameras(), Viewport(0, 0, (float)usedResolution.x(), (float)usedResolution.y()), raycaster);

		// Add views to mvm.
		MultiViewManager        multiViewManager(window, false);
		multiViewManager.addIBRSubView("TM View", texturedView, usedResolution, ImGuiWindowFlags_ResizeFromAnySide);
		multiViewManager.addCameraForView("TM View", generalCamera);

		// Top view
		const std::shared_ptr<sibr::SceneDebugView>    topView(new sibr::SceneDebugView(scene, multiViewManager.getViewport(), generalCamera, myArgs));
		multiViewManager.addSubView("Top view", topView, usedResolution);

		if (myArgs.pathFile.get() !=  "" ) {
			generalCamera->getCameraRecorder().loadPath(myArgs.pathFile.get(), usedResolution.x(), usedResolution.y());
			generalCamera->getCameraRecorder().recordOfflinePath(myArgs.outPath, multiViewManager.getIBRSubView("TM view"), "texturedmesh");
			if( !myArgs.noExit )
				exit(0);
		}

		while (window.isOpened())
		{
			sibr::Input::poll();
			window.makeContextCurrent();
			if (sibr::Input::global().key().isPressed(sibr::Key::Escape))
				window.close();

			multiViewManager.onUpdate(sibr::Input::global());
			multiViewManager.onRender(window);
			window.swapBuffer();
			CHECK_GL_ERROR
		}

	}

	return EXIT_SUCCESS;
}
