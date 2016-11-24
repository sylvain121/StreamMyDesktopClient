// remote desktop sdl client
#include <stdbool.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <SDL/SDL_net.h>


#define INBUF_SIZE 1000000
#define FF_INPUT_BUFFER_PADDING_SIZE 32

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include <stdio.h>

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

struct SendStruct {
	int type;
	int x;
	int y;
	int button;
	int keycode;
};


int main(int argc, char *argv[]) {

	// SDL Event
	SDL_Event userEvent;
	bool quit = false;

	AVFormatContext *pFormatCtx = NULL;
	int             i, videoStream;
	AVCodecContext  *pCodecCtx = NULL;
	AVCodec         *pCodec = NULL;
	AVFrame         *pFrame = NULL;
	float           aspect_ratio;
	struct SwsContext *sws_ctx = NULL;
	AVCodecParserContext *parser = NULL;
	int pts, dts;
	SDL_Overlay     *bmp;
	SDL_Surface     *screen;
	SDL_Rect        rect;
	SDL_Event       event;


	uint8_t inbuf[INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
	memset(inbuf + INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);

  // Register all formats and codecs
	av_register_all();
	avformat_network_init();

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		exit(1);
	}

    // Find the decoder for the video stream
	pCodec=avcodec_find_decoder(AV_CODEC_ID_H264);
	if(pCodec==NULL) {
		fprintf(stderr, "Unsupported codec!\n");
    return -1; // Codec not found
}

  // Copy context
pCodecCtx = avcodec_alloc_context3(pCodec);
pCodecCtx->width = 1920;
pCodecCtx->height = 1080;
pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;


pCodecCtx->flags|= CODEC_FLAG_TRUNCATED;


  // Open codec
if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
    return -1; // Could not open codec

  // Allocate video frame
pFrame=av_frame_alloc();

  // Make a screen to put our video
#ifndef __DARWIN__
screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
#else
screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 32, 0);
#endif
if(!screen) {
	fprintf(stderr, "SDL: could not set video mode - exiting\n");
	exit(1);
}

  // Allocate a place to put our YUV image on that screen
bmp = SDL_CreateYUVOverlay(pCodecCtx->width,
	pCodecCtx->height,
	SDL_YV12_OVERLAY,
	screen);

  // initialize SWS context for software scaling
sws_ctx = sws_getContext(pCodecCtx->width,
	pCodecCtx->height,
	pCodecCtx->pix_fmt,
	pCodecCtx->width,
	pCodecCtx->height,
	PIX_FMT_YUV420P,
	SWS_BILINEAR,
	NULL,
	NULL,
	NULL
	);
/**
*
*
* NETWORK
*
*/


IPaddress ip;
TCPsocket sd;
IPaddress ipControl;
TCPsocket socketControl;
int len;

if(SDLNet_Init() < 0 ) {
	fprintf(stderr, "SDLNet_Init: %s\n", SDLNet_GetError());
	exit(EXIT_FAILURE);
}

if(SDLNet_ResolveHost(&ip, argv[1], atoi(argv[2])) < 0) {
	fprintf(stderr, "unable to resolve address %s , port %s \n", argv[1], argv[2]);
	exit(1);
} 

if(!(sd = SDLNet_TCP_Open(&ip))) {
	fprintf(stderr, "SDLNet_TCP_Open: %s\n", SDLNet_GetError());
	exit(EXIT_FAILURE);	
}

	//getting h264 flow 
printf("sending h264 request packet\n");
char sendKey[1] = {'a'};
len = strlen(sendKey);
SDLNet_TCP_Send(sd, (void * )sendKey, len);



if(SDLNet_ResolveHost(&ipControl, argv[1], atoi(argv[2])) < 0) {
	fprintf(stderr, "unable to resolve address %s , port %s \n", argv[1], argv[2]);
	exit(1);
} 

if(!(socketControl = SDLNet_TCP_Open(&ipControl))) {
	fprintf(stderr, "SDLNet_TCP_Open: %s\n", SDLNet_GetError());
	exit(EXIT_FAILURE);	
}
//activate control
printf("Activate control\n");
char sendKey2[1] = {'b'};
len = strlen(sendKey2);
SDLNet_TCP_Send(socketControl, (void * )sendKey2, len);

	// received data loop
AVPacket        packet;
packet.data = NULL;
packet.size = 0;

parser = av_parser_init(pCodecCtx->codec_id);
parser->flags |= PARSER_FLAG_ONCE;
int parserLenght = 0;
char net_in[INBUF_SIZE];
int inbuf_average = 0;



for(;;) {


		/**
		*
		* sdl event
		*/
		while(SDL_PollEvent(&userEvent)) {

			struct SendStruct send; 
			
			switch(userEvent.type) {
				case SDL_QUIT: 
				quit = true;
				break;
				
				case SDL_KEYDOWN: 
				//printf("pressed key %d\n", userEvent.key.keysym.scancode);
				send.type = 3;
				send.keycode = userEvent.key.keysym.sym;
				break;

				case SDL_KEYUP: 
				//printf("released key %d\n", userEvent.key.keysym.sym);
				send.type = 4;
				send.keycode = userEvent.key.keysym.scancode;
				break;				
				
				case SDL_MOUSEMOTION: 
				//printf("mouse position x: %d, y: %d \n", userEvent.motion.x, userEvent.motion.y);
				send.type = 0;
				send.x = userEvent.motion.x;
				send.y = userEvent.motion.y;
				break;
				case SDL_MOUSEBUTTONDOWN: {
					send.type = 1;
					switch(userEvent.button.button) {
						case SDL_BUTTON_LEFT: {
							//printf("left click down\n");
							send.button = 1;
							break;
						}
						case SDL_BUTTON_RIGHT: {
							//printf("right click down\n");
							send.button = 3;
							break;
						}
						case SDL_BUTTON_MIDDLE: {
							//printf("middle click down\n");
							send.button = 2;
							break;
						}
					}
					break;
				}

				case SDL_MOUSEBUTTONUP:    {
					send.type = 2;
					switch(userEvent.button.button) {
						case SDL_BUTTON_LEFT: {
							//printf("left click released\n");
							send.button = 1;
							break;
						}
						case SDL_BUTTON_RIGHT: {
							//printf("right click released\n");
							send.button = 3;
							break;
						}
						case SDL_BUTTON_MIDDLE: {
							//printf("middle click released\n");
							send.button = 2;
							break;
						}
					}
					break;
				}

			}

			SDLNet_TCP_Send(socketControl, (void * )&send, sizeof(send));

		}

		if(quit)
			break;
		/**
		*
		* Video decode part
		*
		*/
		av_init_packet(&packet);

		int net_lenght =SDLNet_TCP_Recv(sd, net_in, 64);
		memcpy(inbuf+inbuf_average, net_in, net_lenght);
		//printf("inbuf_average %i  + network length %i \n", inbuf_average, net_lenght);
		inbuf_average = net_lenght + inbuf_average;
		//printf("result inbuf_average %i \n", inbuf_average);


		parserLenght = av_parser_parse2(parser, pCodecCtx, &packet.data, &packet.size, &inbuf[0], inbuf_average, pts, dts, AV_NOPTS_VALUE);
		//printf("Parser length %i\n", parserLenght);
		memcpy(inbuf, inbuf+parserLenght, inbuf_average - parserLenght);
		inbuf_average -= parserLenght;

		//printf("inbuf_average %i\n", inbuf_average);
		// if(net_lenght == 0)
		// 	break;

		while(packet.size > 0) {

			int lenght;
			// Decode video frame
			int frameFinished;

			lenght = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
			if( lenght < 0 ) {
				fprintf(stderr, "Error while decoding frame\n");
			}
		      // Did we get a video frame?
			if(frameFinished) {
				SDL_LockYUVOverlay(bmp);

				AVPicture pict;
				pict.data[0] = bmp->pixels[0];
				pict.data[1] = bmp->pixels[2];
				pict.data[2] = bmp->pixels[1];

				pict.linesize[0] = bmp->pitches[0];
				pict.linesize[1] = bmp->pitches[2];
				pict.linesize[2] = bmp->pitches[1];

			// Convert the image into YUV format that SDL uses
				sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
					pFrame->linesize, 0, pCodecCtx->height,
					pict.data, pict.linesize);

				SDL_UnlockYUVOverlay(bmp);

				rect.x = 0;
				rect.y = 0;
				rect.w = pCodecCtx->width;
				rect.h = pCodecCtx->height;
				SDL_DisplayYUVOverlay(bmp, &rect);

			}
			if(packet.data)
			{
				packet.size -=lenght;
				packet.data +=lenght;
			}
			av_free_packet(&packet);
		}

		
	}


  // Free the YUV frame
	av_frame_free(&pFrame);

  // Close the codec
	avcodec_close(pCodecCtx);

  // Close the video file
	avformat_close_input(&pFormatCtx);

	return 0;

}
