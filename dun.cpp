
#include <stdio.h>
#include <stdint.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <sstream>
#include <map>

#include "celfile.h"
#include "cel_frame.h"

#include "min.h"
#include "til.h"


#define WIDTH 1280
#define HEIGHT 960
#define BPP 4
#define DEPTH 32


// How many frames time values to keep
// The higher the value the smoother the result is...
// Don't make it 0 or less :)
#define FRAME_VALUES 10

// An array to store frame times:
Uint32 frametimes[FRAME_VALUES];

// Last calculated SDL_GetTicks
Uint32 frametimelast;

// total frames rendered
Uint32 framecount;

// the value you want
float framespersecond;

// This function gets called once on startup.
void fpsinit() {

    // Set all frame times to 0ms.
    memset(frametimes, 0, sizeof(frametimes));
    framecount = 0;
    framespersecond = 0;
    frametimelast = SDL_GetTicks();

}

void fpsthink() {

    Uint32 frametimesindex;
    Uint32 getticks;
    Uint32 count;
    Uint32 i;

    // frametimesindex is the position in the array. It ranges from 0 to FRAME_VALUES.
    // This value rotates back to 0 after it hits FRAME_VALUES.
    frametimesindex = framecount % FRAME_VALUES;

    // store the current time
    getticks = SDL_GetTicks();

    // save the frame time value
    frametimes[frametimesindex] = getticks - frametimelast;

    // save the last frame time for the next fpsthink
    frametimelast = getticks;

    // increment the frame count
    framecount++;

    // Work out the current framerate

    // The code below could be moved into another function if you don't need the value every frame.

    // I've included a test to see if the whole array has been written to or not. This will stop
    // strange values on the first few (FRAME_VALUES) frames.
    if (framecount < FRAME_VALUES) {

        count = framecount;

    } else {

        count = FRAME_VALUES;

    }

    // add up all the values and divide to get the average frame time.
    framespersecond = 0;
    for (i = 0; i < count; i++) {

        framespersecond += frametimes[i];

    }

    framespersecond /= count;

    // now to make it an actual frames per second value...
    framespersecond = 1000.f / framespersecond;

}


void setpixel_real(SDL_Surface *s, int x, int y, colour c)
{
    y = y*s->pitch/BPP;

    Uint32 *pixmem32;
    Uint32 colour;  
 
    colour = SDL_MapRGB( s->format, c.r, c.g, c.b );
  
    pixmem32 = (Uint32*) s->pixels  + y + x;
    *pixmem32 = colour;
}

void setpixel(SDL_Surface* s, int x, int y, colour c)
{
    setpixel_real(s, x, y, c);
    return;

    setpixel_real(s, x*2, y*2, c);
    setpixel_real(s, x*2+1, y*2, c);
    setpixel_real(s, x*2, y*2+1, c);
    setpixel_real(s, x*2+1, y*2+1, c);
}

void draw_at(SDL_Surface* s, int start_x, int start_y, const Cel_frame& frame)
{
    for(int x = 0; x < frame.width; x++)
    {
        for(int y = 0; y < frame.height; y++)
        {
            if(frame[x][y].visible)
                setpixel(s, start_x+x, start_y+y, frame[x][y]);
        }
    }
}

SDL_Surface* screen;

SDL_Surface** tileset = NULL;

SDL_Surface* create_transparent_surface(size_t width, size_t height)
{
     SDL_Surface* s; 
    
    // SDL y u do dis
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        s = SDL_CreateRGBSurface(SDL_HWSURFACE, width, height, DEPTH, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    #else
        s = SDL_CreateRGBSurface(SDL_HWSURFACE, width, height, DEPTH, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    #endif

    /*s = SDL_CreateRGBSurface(SDL_HWSURFACE, width, height, screen->format->BitsPerPixel,
    screen->format->Rmask,
            screen->format->Gmask,
                    screen->format->Bmask,
                            screen->format->Amask
                            );
*/
    SDL_FillRect(s, NULL, SDL_MapRGBA(s->format, 0, 0, 0, 0)); 

    return s;
}


SDL_Surface* get_sprite(Cel_file& f, size_t index)
{
    if(tileset == NULL)
    {
        size_t size = f.get_num_frames();

        tileset = new SDL_Surface*[size];
        
        for(size_t i = 0; i < size; i++)
            tileset[i] = NULL;
    }
    
    if(tileset[index] != NULL)
        return tileset[index];
    
    Cel_frame frame = f[index];

    SDL_Surface* s = create_transparent_surface(frame.width, frame.height);

    draw_at(s, 0, 0, frame);

    
    tileset[index] = s;

    return s;
}

void draw_surface(SDL_Surface* s, int x, int y)
{
    if(x+s->w > 0 && x < WIDTH && y+s->h > 0 && y < HEIGHT)
    {
        SDL_Rect rcDest = { x, y, 0, 0 };
        SDL_BlitSurface (s , NULL, screen, &rcDest );
    }
}

void blit(SDL_Surface* from, SDL_Surface* to, int x, int y)
{
    SDL_Rect rcDest = { x, y, 0, 0 };
    SDL_BlitSurface (from , NULL, to, &rcDest );
}

int x_base = WIDTH/2, y_base = 0;

void draw_min_tile(SDL_Surface* s, Cel_file& f, int x, int y, int16_t l, int16_t r)
{
    if(l != -1)
        draw_at(s, x, y, f[l]);
        //blit(get_sprite(f, l), s, x+ 0, y);
    if(r != -1)
        draw_at(s, x+32, y, f[r]);
        //blit(get_sprite(f, r), s, x+32, y);
}

void draw_min_pillar(SDL_Surface* s, int x, int y, const MinPillar& pillar, Cel_file& tileset)
{
    // Each iteration draw one row of the min
    for(int i = 0; i < 16; i+=2)
    {
        int16_t l = (pillar[i]&0x0FFF)-1;
        int16_t r = (pillar[i+1]&0x0FFF)-1;
        
        draw_min_tile(s, tileset, x, y, l, r);
    
        y += 32; // down 32 each row

        //std::cout << i << " " << l << " " << r << std::endl;
    }
}

int blits;
std::map<size_t, SDL_Surface*> tilCache;

void draw_til_block(SDL_Surface* to, int x, int y, const TilFile& til, size_t index, const MinFile& min, Cel_file& tileset)
{
    //x += x_base;
    //y += y_base;
    
    //if(x < screen->w && x+128 > 0 && y < screen->h && y+288 > 0)
    {
        SDL_Surface* s;

        if(tilCache.count(index))
            s = tilCache[index];

        else
        {
            s = create_transparent_surface(128, 288);

            draw_min_pillar(s, 32,  0, min[til[index][0]], tileset);
            draw_min_pillar(s,  0, 16, min[til[index][2]], tileset);
            draw_min_pillar(s, 64, 16, min[til[index][1]], tileset);
            draw_min_pillar(s, 32, 32, min[til[index][3]], tileset);

            tilCache[index] = s;
        }
        
        blit(s, to, x, y);
        blits++;
    }
}

SDL_Surface* level = NULL;
void draw_level(int width, int height, uint16_t* tiles, Cel_file town, MinFile min, TilFile til)
{

    if(level == NULL)
    {
        level = SDL_CreateRGBSurface(SDL_HWSURFACE, ((width+height))*64, ((width+height))*32 + 224, screen->format->BitsPerPixel,
                                              screen->format->Rmask,
                                              screen->format->Gmask,
                                              screen->format->Bmask,
                                              screen->format->Amask);

        int x_shift = height*64 - 64;
        int y_shift = 0;
        
        int x = 0;
        int y = 0;
        for(int i = 0; y < width; i++)
        {
            if(tiles[i] != 0)
            {
                draw_til_block(level, (y*(-64)) + 64*x + x_shift, (y*32) + 32*x + y_shift, til, tiles[i]-1, min, town);
            }
            
            x++;

            if(x == height)
            {
                //break;
                //std::cout << "asdasdasdasdasdasdasdasdasd" << std::endl;
                y++;
                x = 0;
            }
        }
    }

    blit(level, screen, x_base, y_base);
}


int main(int argc, char** argv)
{

    FILE * f;

    f = fopen(argv[1], "rb");
    

    uint16_t width;
    fread(&width, 2, 1, f);
    uint16_t height;
    fread(&height, 2, 1, f);
    
    std::cerr << width*height << std::endl;
    uint16_t tiles[width*height];
    fread(&tiles[0], 2, width*height, f);

    for(size_t i = 0; i < width*height; i++)
    {
        //std::cout << i << " " << tiles[i] << std::endl;
    }
    
    fclose(f);

    /*colour pal[256];
    get_pal("../../diabdat.mpq/levels/towndata/town.pal", pal);
    
    FILE * cel_file;
    cel_file = fopen("../../diabdat.mpq/levels/towndata/town.cel", "rb");
    size_t num_frames = get_num_frames(cel_file);
    uint32_t frame_offsets[num_frames+1];
    get_frame_offsets(cel_file, frame_offsets, num_frames);
    */
    
    std::vector<colour> raw_image;
    //bool tile_cel = true;
    //size_t frame_num = 0;


    
    
    
    
    //SDL_Surface *screen;
    SDL_Event event;
  
    int h=0; 
  
    if (SDL_Init(SDL_INIT_VIDEO) < 0 ) return 1;
   
    if (!(screen = SDL_SetVideoMode(WIDTH, HEIGHT, DEPTH, SDL_HWSURFACE | SDL_DOUBLEBUF)))
    {
        SDL_Quit();
        return 1;
    }
    
    //width = 32;

    std::vector<colour> frame2;
    Cel_file town("../../diabdat.mpq/levels/towndata/town.cel");

    //town.get_frame(124, raw_image); 
    
    //town.get_frame(125, frame2); 

   // draw_at(screen, 10, 10, raw_image, width);
    //draw_at(screen, 42, 11, frame2, width);
    //draw_at(screen, 42, 26, raw_image, width);
    
    //SDL_Surface* s = get_sprite(town, 124);

    
    /*draw_tile(town, 124, 125, 0, 0);
    draw_tile(town, 124, 125, 1, 0);
    draw_tile(town, 124, 125, 2, 0);
    draw_tile(town, 124, 125, 2, 1);*/
    
    /* 
    int x = 0, y = 0;
    for(size_t i = 0; i < width*height; i+= 2)
    {
        draw_tile(town, tiles[i], tiles[i+1], x, y);
        
        x++;
        if(x == width)
        {
            x = 0;
            y++;
        }

    }*/
 
    /*FILE* minF = fopen("../../diabdat.mpq/levels/towndata/town.min", "rb");
    //fseek(min, 0, SEEK_END);
    uint16_t min[16];
    fread(&min, 2, 16, minF);

    std::cout << (min[0] & 0x0FFF)-1 << std::endl;
    //draw_tile(town, min[2], min[3], 0, 1);
    //draw_min_tile(town, 124, 125, 0);
    //draw_min_tile(town, 125, -1, 1);*/

    MinFile min("../../diabdat.mpq/levels/towndata/town.min");
    size_t current = 113;

    TilFile til("../../diabdat.mpq/levels/towndata/town.til");

    int lr = 0;
    int ud = 0;

    fpsinit();    
 
    bool quit = false;
    while(!quit)
    {
        while(SDL_PollEvent(&event)){
            switch (event.type) 
            {
                case SDL_QUIT:
	                quit = true;
	                break;

                case SDL_KEYDOWN:
                    switch(event.key.keysym.sym){
                        case SDLK_UP:
                            current++;
                            break;
                        case SDLK_DOWN:
                            current--;
                            break;
                        case SDLK_w:
                            ud--;
                            break;
                        case SDLK_s:
                            ud++;
                            break;
                        case SDLK_a:
                            lr--;
                            break;
                        case SDLK_d:
                            lr++;
                            break;

                    }
                    break;

                case SDL_KEYUP:
                    switch(event.key.keysym.sym){
                         case SDLK_w:
                            ud++;
                            break;
                        case SDLK_s:
                            ud--;
                            break;
                        case SDLK_a:
                            lr++;
                            break;
                        case SDLK_d:
                            lr--;
                            break;
                    }
                    break;


            }
           //width = get_frame(cel_file, pal, frame_offsets, frame_num, raw_image, tile_cel);
        }

        
        if(SDL_MUSTLOCK(screen)) 
        {
            if(SDL_LockSurface(screen) < 0) continue;
        }

        x_base += lr;
        y_base += ud; 
     
        //SDL_FillRect(screen,NULL, SDL_MapRGB( screen->format, 0, 0, 255)); 

        //draw_til_block(0, 0, til[tiles[0]-1], min, town);
        //draw_til_block(64, 32, til[tiles[1]-1], min, town);
        
        blits = 0;

        

        //draw_til_block(0, 0, til, current, min, town);
        
        //draw_til_block(-64, 32, til[tiles[25]-1], min, town);


      
        draw_level(width, height, tiles, town, min, til);
        
        
        /*
        for(int j = 0; j < 256; j++)
        {
            setpixel(screen, j, 0, pal[j]);
        }
        */

        
        if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
      
        SDL_Flip(screen); 
        //while(1);
        
        fpsthink();

        std::stringstream s;
        s << "FPS: " << std::setw(3) << std::setfill('0') << (int)framespersecond << ", blits: " << blits;

        SDL_WM_SetCaption(s.str().c_str(), 0);
    }
}