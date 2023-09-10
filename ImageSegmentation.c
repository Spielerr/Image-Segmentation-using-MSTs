#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <math.h>
#define MAX(a,b) a>b?a:b
#define MIN(a,b) a>b?b:a
#define THRESHOLD(size, c) (c/size)

typedef struct {
  float w;
  int a, b;
} edge;

typedef struct {
  int width;
  int height;   
  int channels;
  int r[2000][1000];
  int g[2000][1000];
  int b[2000][1000];
  // float *data;
} Image_all;

typedef struct {
  int width;
  int height;
  int img[2000][1000];
} Image;

typedef struct { char r, g, b; } rgb;

typedef struct {
  int rank;
  int parent;
  int size;
} subset;


rgb random_rgb(){ 
  rgb c;  
  c.r = (char)rand();
  c.g = (char)rand();
  c.b = (char)rand();

  return c;
}


int find(subset* sub,int x) {
  int y = x;
  while (y != sub[y].parent)
    y = sub[y].parent;
  sub[x].parent = y;
  return y;
}

void join(subset *subsets,int x, int y,int *num) {
  if (subsets[x].rank > subsets[y].rank) {
    subsets[y].parent= x;
    subsets[x].size += subsets[y].size;
  } else {
    subsets[x].parent = y;
    subsets[y].size += subsets[x].size;
    if (subsets[x].rank == subsets[y].rank)
      subsets[y].rank++;
  }
  *(num)--;
}

void swap(edge **xp, edge **yp) 
{ 
    edge* temp = *xp; 
    *xp = *yp; 
    *yp = temp; 
    //printf("Swapped");
} 

void sort(edge* edges, int n) 
{ 
   int i, j; 
   for (i = 0; i < n-1; i++)       
    {
       // Last i elements are already in place    
       for (j = 0; j < n-i-1; j++)
       {
           if (edges[j].w > edges[j+1].w) 
              swap(&edges[j], &edges[j+1]); 
       }
    }
    printf("Sorting done");
} 

Image_all* read_img(char* input_path) 
{
    int width, height, channels;
    // size_t img_size = width * height * channels;
    unsigned char *img = stbi_load(input_path, &width, &height, &channels, 0);
    int img_size = width * height * channels;
    Image_all *im = (Image_all*)malloc(sizeof(Image_all));
    im->width=width;
    im->height=height;
    im->channels=channels;
    int w,h;
    w=h=0;
    for(unsigned char *p = img; p != img + img_size; p += channels) 
    {                
        // printf("%d %d %d\n", *p, *(p+1), *(p+2));
        im->r[h][w] = *p;
        im->g[h][w] = *(p+1);
        im->b[h][w] = *(p+2);
        w++;
        if(w == width)
        {
          w=0;
          h++;
        }
    }
    return im;
  }

float* gaussian(Image* im, float sigma)
{
    int len=(int)ceil(sigma*im->width)+1;
    //printf("%d\n",len);
    float* mask=(float*)malloc(im->width*sizeof(float));
    //printf("before for loop");
    for(int i=0;i<len;i++)
    {
        mask[i]=exp(-0.5*(float)(pow(i/sigma,2)));
    }
    //printf("Just before returning mask");
    return mask;
}

void normalize(float* mask) {
  int len = sizeof(mask)/sizeof(mask[0]);
  float sum = 0;
  for (int i = 1; i < len; i++) {
    sum += fabs(mask[i]);
  }
  sum = 2*sum + fabs(mask[0]);
  for (int i = 0; i < len; i++) {
    mask[i] /= sum;
  }
}

void convolve_even(Image *src,Image *dst, 
        float* mask) {
  int width = src->width;
  //int height = src->height;
  int len = sizeof(mask)/sizeof(mask[0]);
  for (int y = 0; y < src->height; y++) 
  {
    for (int x = 0; x < src->width; x++) 
    {
      float sum = mask[0] * src->img[y][x];
      for (int i = 1; i < len; i++) 
      {
      sum += mask[i] * (src->img[(int)MAX(x-i,0)][y] + src->img[(int)MIN(x+i, width-1)][y]);
      }
      dst->img[x][y] = sum;
    }
  }
  //printf("Finished Convolution\n");
}

Image* smooth(Image *src, float sigma) {
  //printf("In smooth\n");
  //printf("%d \n",src->img[10][10]);
  float* mask=gaussian(src, sigma);
  //printf("Mask created\n");
  normalize(mask);
  //printf("Normalized\n");
  Image *tmp = (Image*)malloc(sizeof(Image));
  tmp -> height = src -> height;
  tmp -> width = src -> width;
  Image *dst = (Image*)malloc(sizeof(Image));
  dst -> height = src -> width; 
  dst -> width = src -> height;
  convolve_even(src,tmp, mask);
  convolve_even(tmp, dst, mask);
  //printf("Convolved Twice\n");
  //printf("Size of dst:%d %d",dst->height,dst->width);
  //printf("%d\n",dst->img[10][10]);
  return dst;
}
// dissimilarity measure between pixels
float diff(Image *r, Image *g,Image *b,
			 int x1, int y1, int x2, int y2) {
  return sqrt((float)(pow((r->img[x1][y1]-r->img[x2][y2]),2)) +
	      (float)(pow((g->img[x1][y1]-g->img[x2][y2]),2)) +
	      (float)(pow((b->img[x1][y1]-b->img[x2][y2]),2)));
}

Image_all *segment_image(Image_all *im, float sigma, float c, int min_size,
			  int *num_ccs)
{
  
    Image* r = (Image*)malloc(sizeof(Image));
    printf("Image Width:%d Image Height:%d\n",im->width,im->height);
    Image* g = (Image*)malloc(sizeof(Image));
    Image* b = (Image*)malloc(sizeof(Image));
    r->width=im->width;
    r->height=im->height;
    memcpy(r->img, im->r, sizeof(r->img));
    g->width=im->width;
    g->height=im->height;
    memcpy(g->img, im->g, sizeof(g->img));
    b->width=im->width;
    b->height=im->height;
    memcpy(b->img, im->b, sizeof(b->img));
    Image *smooth_r=(Image*)malloc(sizeof(Image));
    Image *smooth_g=(Image*)malloc(sizeof(Image));
    Image *smooth_b=(Image*)malloc(sizeof(Image));
    printf("Before smooth\n");
    smooth_r->height=im->height;
    smooth_r->width=im->width;
    //smooth_r->img=r->img;
    smooth_g->height=im->height;
    smooth_g->width=im->width;
    // smooth_g->img=g->img;
    smooth_b->height=im->height;
    smooth_b->width=im->width;
    //printf("%d  %lf\n,",r->img[10][10], sigma);
    smooth_r = smooth(r, sigma);
    printf("%d\n", smooth_r->img[10][10]);
    //printf("Smoothed R\n %d\n",smooth_r->img[100][100]);
    smooth_g = smooth(g, sigma);
    printf("%d\n", smooth_g->img[10][10]);
    //printf("Smooth r and g\n");
    smooth_b = smooth(b, sigma);
    printf("%d\n", smooth_b->img[10][10]);
    printf("After Smooth\n");  
    edge *edges = (edge*)malloc(im->width*im->height*4*sizeof(edge));
    //edge *edges[im->width*im->height*4];
    int num = 0;
    for (int y = 0; y < im->height; y++) {
      for (int x = 0; x < im->width; x++) {
        if (x < im->width-1) {
    edges[num].a = y * im->width + x;
    edges[num].b = y * im->width + (x+1);
    edges[num].w = diff(smooth_r, smooth_g, smooth_b, x, y, x+1, y);
    num++;
        }

        if (y < im->height-1) {
    edges[num].a = y * im -> width + x;
    edges[num].b = (y+1) * im->width + x;
    edges[num].w = diff(smooth_r, smooth_g, smooth_b, x, y, x, y+1);
    num++;
        }

        if ((x < im->width-1) && (y < im->height-1)) {
    edges[num].a = y * im->width + x;
    edges[num].b = (y+1) * im->width + (x+1);
    edges[num].w = diff(smooth_r, smooth_g, smooth_b, x, y, x+1, y+1);
    num++;
        }

        if ((x < im->width-1) && (y > 0)) {
    edges[num].a = y * im->width + x;
    edges[num].b = (y-1) * im->width + (x+1);
    edges[num].w = diff(smooth_r, smooth_g, smooth_b, x, y, x+1, y-1);
    num++;
        }
      }
      
    }
    printf("%lf %d %d\n", edges[10].w, edges[10].a, edges[10].b);


  int num_edges=num;
  int num_vertices=im->width*im->height;
  printf("Before sorting");

  void quicksort(edge *edges,int first,int last){
     int i, j, pivot;
     if(first<last){
        pivot=first;
        i=first;
        j=last;

        while(i<j){
           // while(number[i]<=number[pivot]&&i<last)
           //    i++;
          while(edges[i].w<=edges[pivot].w && i<last)
            i++;
           // while(number[j]>number[pivot])
           //    j--;
          while(edges[j].w>edges[pivot].w)
            j--;
           if(i<j){
              float tw = edges[i].w;
              edges[i].w = edges[j].w;
              edges[j].w = tw;
              int ta = edges[i].a;
              edges[i].a = edges[j].a;
              edges[j].a = ta;
              int tb = edges[i].b;
              edges[i].b = edges[j].b;
              edges[j].b = tb;
           }
        }

              float tw = edges[pivot].w;
              edges[pivot].w = edges[j].w;
              edges[j].w = tw;
              int ta = edges[pivot].a;
              edges[pivot].a = edges[j].a;
              edges[j].a = ta;
              int tb = edges[pivot].b;
              edges[pivot].b = edges[j].b;
              edges[j].b = tb;
        quicksort(edges,first,j-1);
        quicksort(edges,j+1,last);

     }
   }
  quicksort(edges,0,num);
    printf("%lf %d %d\n", edges[50].w, edges[50].a, edges[50].b);
    printf("%lf %d %d\n", edges[10].w, edges[10].a, edges[10].b);
    printf("%lf %d %d\n", edges[20].w, edges[20].a, edges[20].b);


  subset *subsets = 
        (subset*) malloc( im->width*im->height*sizeof(subset) ); 
  
    // Create V subsets with single elements 
    for (int v = 0; v < im->width*im->height; ++v) 
    { 
        subsets[v].parent = v; 
        subsets[v].rank = 0; 
        subsets[v].size=1;
    } 
  // make a disjoint-set forest
  //universe *u = new universe(num_vertices);

  // init thresholds
  float threshold[num_vertices];
  for (int i = 0; i < num_vertices; i++)
    threshold[i] = THRESHOLD(1,c);

//   // for each edge, in non-decreasing weight order...
  for (int i = 0; i < num; i++) {
    edge *pedge = &edges[i];
    //printf("%d\n",pedge->a);
//     // components conected by this edge
    int a = find(subsets,pedge->a);
    int b = find(subsets,pedge->b);
    if(i==10){printf("A:%d B: %d\n", a, b);}
    if (a != b) {
      if ((pedge->w <= threshold[a]) &&
    (pedge->w <= threshold[b])) {
  join(subsets,a, b,&num);
  a = find(subsets,a);
  threshold[a] = pedge->w + THRESHOLD(subsets[a].size,c);
 // threshold[a] = pedge->w + (u->size(a)/ c);
      }
    }
  }
    for (int i = 0; i < num; i++) {
    int a = find(subsets,edges[i].a);
    int b = find(subsets,edges[i].b);
    if ((a != b) && ((subsets[a].size < min_size) || (subsets[b].size< min_size)))
      join(subsets,a, b,&num);
  }

    Image_all *output = (Image_all*)malloc(sizeof(Image_all));

  // pick random colors for each component
  // rgb *colors = rgb[im->width*im->height];
    printf("before malloc\n");
    rgb *colors = (rgb*)malloc((im->width*im->height*sizeof(rgb)));
    printf("after malloc\n");
  for (int i = 0; i < im->width*im->height; i++)
    colors[i] = random_rgb();
  
  for (int y = 0; y < im->height; y++) {
    for (int x = 0; x < im->width; x++) {
      int comp = find(subsets,y * im->width + x);
      // printf("Comp:%d\n",comp );
      output->r[x][y] = (int)colors[comp].r;
      output->g[x][y] = (int)colors[comp].g;
      output->b[x][y] = (int)colors[comp].b;
    }
  }  
  printf("Checkpoint:1 %d\n",im->width*im->height*im->channels);
  unsigned char* output_img =malloc(im->width*im->height*im->channels);
  printf("Check\n");
  int w,h;
  w=h=0;
  for(unsigned char *pg = output_img; ;pg += im->channels) {
         // *pg = (uint8_t)((*p + *(p + 1) + *(p + 2))/3.0);
        *pg = (uint8_t)output->r[h][w];
        *(pg+1) = (uint8_t)output->g[h][w];
        *(pg+2) = (uint8_t)output->b[h][w];
        w++;
        if(w == im->width)
        {
          w=0;
          h++;
         }
         if(h == im->height)
          break;
        }
        printf("test :%d\n",w );
     

    printf("Im working\n");
    // return r;
    stbi_write_jpg("sky.jpg", im->width, im->height, im->channels, output_img, 80);
    stbi_image_free(output_img);
    return output;
  }

  int main()
  {
    Image_all* im = read_img("sky.PPM");
    printf("before fn call");
    int numcss;
    segment_image(im, 0.5, 500, 20,&numcss);
    // printf("numcss : %d", numcss)
    printf("end of main");
    return 0;
  }
