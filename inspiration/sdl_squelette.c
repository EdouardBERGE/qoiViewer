#include <SDL2/SDL.h>



void main(int argc, char **argv) {
	SDL_Surface *screen,*surface;
	SDL_Window *win;
	int i=0,redo=1;
	int gmx,gmy,ms;
	char *state;
	int refx,refy;
	int dekx,deky;
	int getx,gety;

	if (SDL_Init(SDL_INIT_EVERYTHING)!=0) {
			fprintf(stderr,"SDL_Init error [%s]\n",SDL_GetError());
			return;
	}

	//SDL_SendKeyboardKey(SDL_PRESSED , SDL_SCANCODE_F11);
	//SDL_SendKeyboardKey(SDL_RELEASED, SDL_SCANCODE_F11);
        printf("number of displays = %d\n",SDL_GetNumVideoDisplays());
        for (i=0;i<SDL_GetNumVideoDisplays();i++) {
                SDL_Rect rect;
                if (!SDL_GetDisplayBounds(i,&rect)) {
                        printf("Display %d is located %dx%d with size %dx%d\n",i,rect.x,rect.y,rect.w,rect.h);
                } else {
                        printf("Display %d ERROR\n",i);
                }
        }

	refx=refy=200;
	dekx=deky=0;

	if ((surface=SDL_CreateRGBSurface(0,1800,900,32,0,0,0,0))==NULL) { // default mask for 32 bits pixels
			fprintf(stderr,"SDL_CreateRGBSurface error\n");
			return;
	}

	if ((win=SDL_CreateWindow("Auto", 0, 0, 40,40, SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS))==NULL) { fprintf(stderr,"SDL_CreateWindow error\n"); return; }
	screen=SDL_GetWindowSurface(win);
	SDL_BlitSurface(surface,NULL,screen,NULL);
	SDL_UpdateWindowSurface(win);
	SDL_PumpEvents();
	SDL_GetWindowPosition(win,&getx,&gety); printf("getx=%d gety=%d\n",getx,gety);

	refx=getx;refy=gety;
	SDL_DestroyWindow(win);

	if ((win=SDL_CreateWindow("Auto", 0, 0, 40,40, SDL_WINDOW_SHOWN))==NULL) { fprintf(stderr,"SDL_CreateWindow error\n"); return; }
	screen=SDL_GetWindowSurface(win);
	SDL_BlitSurface(surface,NULL,screen,NULL);
	SDL_UpdateWindowSurface(win);
	SDL_PumpEvents();
	SDL_GetWindowPosition(win,&getx,&gety); printf("getx=%d gety=%d\n",getx,gety);

	refx-=getx; refy-=gety; printf("refx=%d refy=%d\n",refx,refy);
	SDL_DestroyWindow(win);

	if ((win=SDL_CreateWindow("Auto", 200+refx, 200+refy, 40,40, SDL_WINDOW_SHOWN))==NULL) { fprintf(stderr,"SDL_CreateWindow error\n"); return; }
	screen=SDL_GetWindowSurface(win);
	SDL_BlitSurface(surface,NULL,screen,NULL);
	SDL_UpdateWindowSurface(win);
	SDL_PumpEvents();
	SDL_GetWindowPosition(win,&getx,&gety); printf("getx=%d gety=%d\n",getx,gety);

//exit(0);


	state=SDL_GetKeyboardState(NULL);

struct s_joystick {
        SDL_Joystick *host_joy;
        SDL_JoystickID joyid; // =SDL_JoystickInstanceID(SDL_Joystick *joystick);
        // extra info
        char *name;
        char *match; //???
        int nbaxis,nbbutton;
};

struct s_joystick host_joy[16];
int nbjoy;

int foule=0;

	nbjoy=SDL_NumJoysticks();
	if (nbjoy>16) nbjoy=16;

	for (i=0;i<nbjoy;i++) {
		const char *gcname;
		gcname=SDL_GameControllerNameForIndex(i);
		if (!gcname) gcname=strdup("No name!");
		printf("USB controller %d => %s\n",i,gcname);

		host_joy[i].host_joy=SDL_JoystickOpen(i);
		if (host_joy[i].host_joy) {
			host_joy[i].joyid=SDL_JoystickInstanceID(host_joy[i].host_joy);
			host_joy[i].name=gcname;
			host_joy[i].nbaxis=SDL_JoystickNumAxes(host_joy[i].host_joy);
			host_joy[i].nbbutton=SDL_JoystickNumButtons(host_joy[i].host_joy);
			//host_joy[i].definition=E_JOYSTICK_REAL+i; // store index!
		}
	}



	while (redo) {
		SDL_DisplayMode mode;
		SDL_Event event;
		unsigned int *membitmap;
		int i;
		SDL_LockSurface(surface);
		membitmap=surface->pixels;

		if (!foule) {
			memset(membitmap,i++,40*40*sizeof(unsigned int));
		} else {
			memset(membitmap,i++,mode.w*mode.h*sizeof(unsigned int));
		}

		SDL_UnlockSurface(surface);
		SDL_BlitSurface(surface,NULL,screen,NULL);
		SDL_UpdateWindowSurface(win);

		/* interactivité */
		//SDL_PumpEvents();
		//ms=SDL_GetMouseState(&gmx,&gmy);
		
		SDL_PollEvent(&event);
		if (event.type==SDL_KEYDOWN) {
			printf("down for scancode %d\n",event.key.keysym.scancode);
			if (event.key.keysym.scancode==SDL_SCANCODE_ESCAPE) redo=0;
		} else if (event.type==SDL_KEYUP) {
			printf("up for scancode %d\n",event.key.keysym.scancode);
			if (event.key.keysym.scancode==SDL_SCANCODE_ESCAPE) redo=0;
		} else if (event.type==SDL_WINDOWEVENT) {
			switch (event.window.event) {
				default:printf("unknown windowevent %d\n",event.window.event);break;
				case SDL_WINDOWEVENT_NONE:printf("none\n");break;
				case SDL_WINDOWEVENT_SHOWN:printf("shown\n");break;
				case SDL_WINDOWEVENT_HIDDEN:printf("hidden\n");break;
				case SDL_WINDOWEVENT_EXPOSED:printf("exposed\n");
							     break;
				case SDL_WINDOWEVENT_MOVED:
SDL_GetWindowPosition(win,&getx,&gety);
					     printf("moved %dx%d    get=%d/%d\n",event.window.data1,event.window.data2,getx,gety);


							   if (event.window.data1==0 && event.window.data2==0) {
		  printf("*****************************************\n");
		  printf("foulescreen!\n");
		  printf("*****************************************\n");

printf("nuimber of display => %d\n",SDL_GetNumVideoDisplays());
if (!SDL_GetCurrentDisplayMode(0,&mode)) {
	printf("current mode is %d x %d at %d Hz\n",mode.w,mode.h,mode.refresh_rate);
}

			if (SDL_GetWindowFlags(win)&SDL_WINDOW_FULLSCREEN_DESKTOP) {
				printf("FOULE confirmed!\n");
			} else {
				printf("no FS flag\n");
			}
SDL_SetWindowFullscreen(win,SDL_WINDOW_FULLSCREEN_DESKTOP);
SDL_SetWindowAlwaysOnTop(win,1);

                                SDL_FreeSurface(surface);
                                if ((surface=SDL_CreateRGBSurface(0,mode.w,mode.h,32,0,0,0,0))==NULL) printf("SDL_CreateRGBSurface error on overscan resize to %dx%d\n",mode.w,mode.h);
                                // resizing win
                                printf("resizing win to %dx%d\n",mode.w,mode.h);
                                SDL_SetWindowSize(win,mode.w,mode.h);
                                printf("Get new screen surface\n");
                                screen=SDL_GetWindowSurface(win);
				foule=1;
							   } else {
								   if (foule) {
									   foule=0;
                                SDL_FreeSurface(surface);
                                if ((surface=SDL_CreateRGBSurface(0,800,600,32,0,0,0,0))==NULL) printf("SDL_CreateRGBSurface error on overscan resize\n");
                                // resizing win
		  printf("-----------------------------------------\n");
SDL_SetWindowAlwaysOnTop(win,0);
                                printf("resizing win to %dx%d\n",800,600);
                                SDL_SetWindowSize(win,800,600);
                                printf("Get new screen surface\n");
                                screen=SDL_GetWindowSurface(win);

								   }
							   }
							   break;
				case SDL_WINDOWEVENT_RESIZED:printf("resized\n");break;
				case SDL_WINDOWEVENT_SIZE_CHANGED:printf("size changed\n");break;
				case SDL_WINDOWEVENT_MINIMIZED:printf("minimized\n");break;
				case SDL_WINDOWEVENT_MAXIMIZED:printf("maximized\n");break;
				case SDL_WINDOWEVENT_RESTORED:printf("restored\n");break;
				case SDL_WINDOWEVENT_ENTER:printf("enter\n");break;
				case SDL_WINDOWEVENT_LEAVE:printf("leave\n");break;
				case SDL_WINDOWEVENT_FOCUS_GAINED:printf("focus in\n");break;
				case SDL_WINDOWEVENT_FOCUS_LOST:printf("focus out\n");break;
				case SDL_WINDOWEVENT_CLOSE:printf("close!\n");break;
				case SDL_WINDOWEVENT_TAKE_FOCUS:printf("take focus\n");break;
				case SDL_WINDOWEVENT_HIT_TEST:printf("hit test\n");break;
				case SDL_WINDOWEVENT_ICCPROF_CHANGED:printf("iccprofile\n");break;
				case SDL_WINDOWEVENT_DISPLAY_CHANGED:printf("display change\n");break;
			}


			{
				SDL_Renderer *render;
				int w,h;

				render=SDL_GetRenderer(win);
				if (render && !SDL_GetRendererOutputSize(render,&w,&h)) {
					printf("GetRenderSize = %d x %d\n",w,h);
				}
			}

		} else if (event.type==SDL_DROPBEGIN) {
			printf("begin drop\n");
		} else if (event.type==SDL_DROPFILE) {
			printf("dropfile\n");
		} else if (event.type==SDL_DROPCOMPLETE) {
			printf("drop complete\n");
		} else if (event.type==SDL_JOYAXISMOTION) {
                                // we must avoid the glitch of the centered position not always to zero
                                // we must allow the use of analog joystick (interval from -32K to 32K)
				static int previous=0;
				if (abs(previous)<4000 && abs(event.jaxis.value)>4000) printf("joy jaxis.which=%d axis=%d value=%d\n",event.jaxis.which,event.jaxis.axis,event.jaxis.value);
				if (abs(previous)>4000 && abs(event.jaxis.value)<4000) printf("joy jaxis.which=%d axis=%d value=%d\n",event.jaxis.which,event.jaxis.axis,event.jaxis.value);
				previous=event.jaxis.value;
		} else if (event.type==SDL_JOYBUTTONDOWN || event.type==SDL_JOYBUTTONUP) {
				printf("joy button which %d button %d state %d\n",event.jbutton.which,event.jbutton.button,event.jbutton.state);
		} else {
		}

	}
	printf("%d\n",i);
	SDL_FreeSurface(surface);
	SDL_DestroyWindow(win);
	SDL_Quit();
}

