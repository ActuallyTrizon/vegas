 
 ---
 ### THIS IS THE OFFICIAL BUILD OF STAR ENGINE RECENTLY REBRANDED TO **VEGAS** RECNT BULDS ABOVE V2.7.2.1.
        * This is the major build structure for Windows.
 
 ---
 ### **FOLDERS**{
                  **StarEngine_Release:** 
				  (this is where the VEGAS DLL BUILDS is exported)
				  
				  **patches:** 
				  (contains the patch of modified dxvk version)
				  
				  **ndk:** 
				  (contains the ndk sdk for building the project)
				  
				  **dxvk-source:** 
				  (the dxvk file to be patched or modified lives here. note that for every dxvk project pulled or downloaded, 
				  rename to ```dxvk``` and place them in this directory)
				  
				  **build-script:**
				  (contains custom tuned build commands tailoered for windows){
				                      
									  * *build_android.sh:*(this is the original build config for the project)
									  * *optional build script(DON'T TOUCH UNLESS IMPORTANT)*[
									                                                           * *apply_star_logic.sh:* (this is star engine code patch for the dxvk project, also remember to 
																							                             rename the ```PATH_FILE``` if the path is different or the patch name is different)
																							   * *build_android(Clang).bat:*(this is incomplete for the **starengine code base** but contains complete build
																							                                 command tailored for ANDROID build **natively**)
																							   * *build_star_engine.sh:*(this is the command build for dxvk when using msys2, especially for the **UCRT64**.)
																							 ]
																						
																						} *(do note that the 32 bit dll command build isn't made yet)*	 
 }
 
 ---
 
>CREDITS: 
>DOITSUJIN(MAINTAINER AND FOUNDER OF DXVK), ISYGOLD(LEAD DEV FOR STAR ENGINE DXVK AND VEGAS, 
>GEMINI(ASSISTANT IN COMPILATION BUILDS AND AGGRESSIVE ERRORS FIXES AND HELP WITH THE ADVANCED CODING).
---

* **COLLABORATORS:**
JACOJJAY, TANAKORN(DEV OF FROST EMULATOR)