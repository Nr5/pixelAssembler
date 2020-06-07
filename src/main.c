#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
//#include <gtksourceview/gtksourceview.h>

#include "tegtkgl.h"
#include <GL/gl.h>
#include <math.h>
#include <SDL/SDL.h>
#include <FreeImage.h>
#include "gifenc.h"
#include <gmodule.h>
ge_GIF * gif;
GtkWidget *win;
#define IS_BIG_ENDIAN (!*(unsigned char *)&(uint16_t){1})
static gboolean do_the_gl = TRUE;
static GtkWidget *g_gl_wid = 0;
uint8_t rendergif=0;
		uint8_t *gif_palette=0;
		
		/*
		{
			0x00 , 0x00 , 0x00 , 
			0x00 , 0x00 , 0xaa , 
			0x00 , 0xaa , 0x00 , 
			0x00 , 0xaa , 0xaa , 
			
			0xaa , 0x00 , 0x00 , 
			0xaa , 0x00 , 0xaa , 
			0xaa , 0x55 , 0x00 ,  
			0xaa , 0xaa , 0xaa , 
			
			0x55 , 0x55 , 0x55 , 
			0x55 , 0xff , 0xff , 
			0x55 , 0xff , 0x55 , 
			0xff , 0x55 , 0x55 , 
			
			0x55 , 0x55 , 0xff , 
			0xff , 0x55 , 0xff ,  
			0xff , 0xff , 0x55 , 
			0xff , 0xff , 0xff  
	};
	*/
		
		uint32_t colortable=0;/*={
			0x000000,
			0xaa0000,
			0x00aa00,
			0xaaaa00,
			
			0x0000aa,
			0xaa00aa,
			0x0055aa,
			0xaaaaaa,
			
			0x555555,
			0xffff55,
			0x55ff55,
			0x5555ff,
			
			0xff5555,
			0xff55ff,
			0x55ffff,
			0xffffff
	};*/
	
  GtkWidget* textViews[8];
  GtkWidget* menubar;
  GtkWidget* imglist_widget;
  
static unsigned int shader;
unsigned char time_0=0;
char paused = 0;
char p2=1;
short registers[64];
SDL_Surface* images[128];
uint8_t n_layers=0;
uint8_t n_images=0;
int16_t compval=0;

typedef struct layer layer;

typedef struct instruction{
	void (*func)(layer*,short arg1,short arg2);
	short arg1;
	short arg2;
	uint8_t a1_type;
	uint8_t a2_type;
	
}instruction;


/*
typedef struct layerlist{
	struct layerlist* next;
	struct layerlist* prev;
	SDL_Surface* img;
	uint8_t wait;
	unsigned char n_instr;
	instruction instr[16];
}layerlist;
*/

typedef struct VarMapPair{
	const char* key;
	int16_t val;
}VarMapPair;
uint16_t framenr=0;
VarMapPair var_map[256];
var_map_size=0;

int16_t getVar(VarMapPair *var_map,uint8_t var_map_size,const char* name) {
	for (int i=0;i<var_map_size;i++){
		if (!strcmp(var_map[i].key ,name)){
			return var_map[i].val;
		}
	}
	return -1;
}

struct layer{
	//layerlist* list;
	//layerlist* current;
	instruction instr[1024];
	short registers[64];
	uint16_t n_instr;
	SDL_Surface* img;
	uint8_t wait;
	uint16_t instr_p;
	unsigned int texture;
	int16_t posx;
	int16_t posy;
	uint16_t shiftx;
	uint16_t shifty;
	VarMapPair var_map[256];
	uint8_t var_map_size;
};

void setimage (layer* this,short arg1,short arg2){
	this->registers[63]=arg1;
	if ( ((uint16_t)arg1) < n_images){
		this->img=images[arg1];
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB ,0,GL_RED,this->img->w,this->img->h,
						0,GL_RED,GL_UNSIGNED_BYTE,this->img->pixels);
	}
}

void drawinstr (layer* this,short arg1,short arg2){
	//setimage (this,this->registers[63],0);
	this->wait=arg1;
	
}

void compare (layer* this,short arg1,short arg2){
	compval=arg1-arg2;
}
void jump_not_equal (layer* this,short arg1,short arg2){
	if (compval && ( ((uint16_t)arg1) < this->n_instr)  ) this->instr_p=arg1-1;
}

void jump_equal (layer* this,short arg1,short arg2){
	if (!compval && ( ((uint16_t)arg1) < this->n_instr)) this->instr_p=arg1-1;
}

void jump_greater_then (layer* this,short arg1,short arg2){
	if (compval>0 && ( ((uint16_t)arg1) < this->n_instr)) this->instr_p=arg1-1;
}

void jump_less_then (layer* this,short arg1,short arg2){
	if (compval<0 && ( ((uint16_t)arg1) < this->n_instr)) this->instr_p=arg1-1;
}

void jump (layer* this,short arg1,short arg2){
	if ( ((uint16_t)arg1) < this->n_instr) this->instr_p=arg1-1;
}
void move (layer* this,short arg1,short arg2){
	this->posx+=arg1;
	this->posy+=arg2;
	
}
void set (layer* this,short arg1,short arg2){
	registers[arg1]=arg2;
}

void add (layer* this,short arg1,short arg2){
	registers[arg1]+=arg2;
}



void shift (layer* this,short arg1,short arg2){
	this->shiftx+=arg1;
	this->shifty+=arg2;
}


typedef struct MapPair{
	const char* key;
	void (*val)(layer* this,short arg1,short arg2);
}MapPair;
MapPair instr_map[]={
	{"add",add},
	{"cmp",compare},
	{"drw",drawinstr},
	{"img",setimage},
	{"jeq",jump_equal},
	{"jgt",jump_greater_then},
	{"jlt",jump_less_then},
	{"jmp",jump},
	{"jne",jump_not_equal},
	{"mov",move},
	{"set",set},
	{"shf",shift}
		
};
uint8_t instr_map_size=sizeof(instr_map)/sizeof(instr_map[0]);


int (*getFunc(const char* name))(layer*,short, short) {
	for (int i=0;i<instr_map_size;i++){
		if (!strcmp(instr_map[i].key ,name)){
			return instr_map[i].val;
		}
	}
	return 0;
}





layer layers[16];
char path_to_res[0];
char* path_from_res;

static void pause_func(GtkWidget *bt, gpointer ud){
		paused=!paused;
		
}
static void load_func(GtkWidget *bt, gpointer ud){
	
			GtkWidget *dialog;
GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
gint res;

dialog = gtk_file_chooser_dialog_new ("Open File",
                                      win,
                                      action,
                                      "Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "Open",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);

GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
    
	gtk_file_chooser_set_current_folder (chooser, path_to_res);
res = gtk_dialog_run (GTK_DIALOG (dialog));
if (res == GTK_RESPONSE_ACCEPT)
  {
    const char*  filename = gtk_file_chooser_get_filename (chooser);
    
	uint16_t header[8];
	
	FILE* fp = fopen(filename,"r");
	
	fread(header,2,8,fp);
	
	
	for (uint8_t i=0;i<8;i++){
		printf("size:%i\n",header[i]);
	
		const char* strbuf = malloc(header[i]);
		fread (strbuf,1,header[i],fp);
		
		
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textViews[i]));
		gtk_text_buffer_set_text (buffer,
                        strbuf,
                        header[i]);
		
	}
		
	fclose(fp);	
	g_free (filename);
  }
	gtk_widget_destroy (dialog);
		
}


static void load_images_func_helper(const char* name,gpointer _){
	SDL_Surface* img=SDL_LoadBMP(name);
	
	for (const char* name2;name2=strstr(name,"/");name=name2+1);
	
	strstr(name,".")[0]=0;
	
	VarMapPair vmp={name,n_images};
		
	var_map[var_map_size++]=vmp;
	
	images[n_images++]=img;
	printf("vmp: %s,%i\n"  ,vmp.key,vmp.val);
	printf("var_map_size: %i\n",var_map_size);
	printf("n_images: %i\n",n_images);
	
	GtkWidget* label= gtk_label_new(name);
	gtk_label_set_xalign (label, 0);
	gtk_container_add(GTK_CONTAINER(imglist_widget), label);
    
}

static void load_images_func(GtkWidget *bt, gpointer ud){
	GtkWidget *dialog;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
	gint res;

	dialog = gtk_file_chooser_dialog_new ("Open Files",
											win,
											action,
											"Cancel",
											GTK_RESPONSE_CANCEL,
											"Open",
											GTK_RESPONSE_ACCEPT,
											NULL);

	GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
	gtk_file_chooser_set_current_folder (chooser, path_to_res);
	gtk_file_chooser_set_select_multiple (chooser, 1 );
	GtkFileFilter * filter =gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter,"*.bmp");
	gtk_file_filter_set_name (filter,"Bitmaps (*.bmp)");
	gtk_file_chooser_add_filter (chooser, filter);
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_ACCEPT)
	{
		GSList * filelist;
		
		
		filelist = gtk_file_chooser_get_filenames (chooser);
		
		
		g_slist_foreach (filelist,
					load_images_func_helper,
					0);
		
		
		gtk_widget_show_all(imglist_widget);
		//g_free (filename);
	}
		gtk_widget_destroy (dialog);
		if(res== GTK_RESPONSE_CANCEL)return;		
			
	
	printf("col0: %i\n",(images[0]->format->palette->colors)[0].g);
	printf("col1: %i\n",((uint32_t*)images[0]->format->palette->colors)[1]);
	printf("col2: %i\n",((uint32_t*)images[0]->format->palette->colors)[2]);
	
	printf("nc: %i\n",((uint32_t*)images[0]->format->palette->ncolors));
	
	if(!colortable){
		colortable=((uint32_t*)images[0]->format->palette->colors);
		
		glUniform1uiv( glGetUniformLocation(shader,"palette") ,images[0]->format->palette->ncolors,
		(uint32_t*) images[0]->format->palette->colors
		);
		
		gif_palette=malloc(images[0]->format->palette->ncolors *3 );
		
		for (int i=0;i<images[0]->format->palette->ncolors;i++){
			gif_palette[i*3]=images[0]->format->palette->colors[i].r;
			gif_palette[i*3+1]=images[0]->format->palette->colors[i].g;
			gif_palette[i*3+2]=images[0]->format->palette->colors[i].b;
			
		}
			
		
		
	}
}

static void save_func(GtkWidget *bt, gpointer ud){
			GtkWidget *dialog;
GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
gint res;

dialog = gtk_file_chooser_dialog_new ("Save File",
                                      win,
                                      action,
                                      "Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "Save",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);

res = gtk_dialog_run (GTK_DIALOG (dialog));
if (res == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
	
	gtk_file_chooser_set_current_folder (chooser, path_to_res);
    filename = gtk_file_chooser_get_filename (chooser);
    
	uint16_t header[8];
	FILE* fp = fopen(filename,"w");
	for (uint8_t i=0;i<8;i++){
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textViews[i]));
	
		header[i]=gtk_text_buffer_get_char_count (buffer);
	
		printf("%i\n",header[i]);
		
	}
	fwrite(header,2,8,fp);
	
	for (uint8_t i=0;i<8;i++){
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textViews[i]));
		
		GtkTextIter start_iter;
		GtkTextIter end_iter;
		gtk_text_buffer_get_start_iter(buffer,&start_iter);
		gtk_text_buffer_get_end_iter(buffer,&end_iter);
                          
		const char* text=	gtk_text_buffer_get_text (buffer,&start_iter,&end_iter, 1);
		printf("%s\n",text);
		fputs (text,fp);
	}
	
	
	
	fclose(fp);	
	
	g_free (filename);
  }
	gtk_widget_destroy (dialog);
		
}



static void destroy_the_gl(GtkWidget *wid, gpointer ud) {
    do_the_gl = FALSE;
}
static gboolean export_gif(){
				GtkWidget *dialog;
GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
gint res;

dialog = gtk_file_chooser_dialog_new ("Save File",
                                      win,
                                      action,
                                      "Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "Save",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);

res = gtk_dialog_run (GTK_DIALOG (dialog));
if (res == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
	
	gtk_file_chooser_set_current_folder (chooser,  path_to_res);
    filename = gtk_file_chooser_get_filename (chooser);
    
	rendergif=1;
	framenr=0;
	gif=ge_new_gif(
        filename,                  /* GIF file name */
        128, 128,    /* frame size */
        gif_palette, 4,        /* color table */
        0                            /* looping information */
    );
	
	
	g_free (filename);
  }
	gtk_widget_destroy (dialog);
	
	
}

static gboolean on_clicked(GtkWidget *wid, GdkEvent *ev, gpointer user_data) {
    printf("clicked at %.3fx%.3f with button %d\n",
        ev->button.x, ev->button.y, ev->button.button);
    
	export_gif();
	
	return TRUE;
}




gboolean draw_the_gl(gpointer ud) {
	if (paused) return TRUE; 
	//if (p2) paused=1;
	//p2=0;
    static float s = 0.f;
    GtkWidget *gl = g_gl_wid;
    GdkWindow *wnd;

    if (!do_the_gl)
        return FALSE;

    // this is very important, as OpenGL has a somewhat global state. 
    // this will set the OpenGL state to this very widget.
    te_gtkgl_make_current(TE_GTKGL(gl));

    for (int i=0;i<1+63*rendergif;i++){
	glUseProgram(shader);
	
	glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if(rendergif){
		glViewport(0, 0, 128, 128);
	}else{
		glViewport(0, 0, 1024, 512);
	}
	
	glLoadIdentity();
    glOrtho(0, 64, 64, 0, 0, 1.0);
	
	float modelview[16];
	glMatrixMode(GL_MODELVIEW);
	glGetFloatv(GL_MODELVIEW_MATRIX,modelview);
	
	glUniformMatrix4fv( glGetUniformLocation(shader,"matrix") ,1,0,modelview );
	glUniform1ui( glGetUniformLocation(shader,"gif") ,rendergif );
	
	
	glClearColor(0, 0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
		
	for (char i=0;i<n_layers;i++){
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB ,layers[i].texture);
		
		while(!(layers[i].wait)  && (layers[i].instr_p < layers[i].n_instr) ){
			instruction instr=layers[i].instr[layers[i].instr_p];
			instr.func(layers+i,
						instr.a1_type ? registers[instr.arg1] :instr.arg1,
						instr.a2_type ? registers[instr.arg2] :instr.arg2
						
  					);
			layers[i].instr_p++;
		}
		layers[i].wait--;
		
	}
		
	for (char i=0;i<n_layers;i++){
		if(!layers[i].img)continue;	
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB ,layers[i].texture);
		
	
		layers[i].shiftx=(layers[i].shiftx+layers[i].img->w) % layers[i].img->w;
		layers[i].shifty=(layers[i].shifty+layers[i].img->h) % layers[i].img->h;
		
		glUniform2ui( glGetUniformLocation(shader,"shift") ,layers[i].shiftx,layers[i].shifty );
		glUniform2ui( glGetUniformLocation(shader,"size") ,layers[i].img->w,layers[i].img->h );
		
		
		int16_t x=layers[i].posx;
		int16_t y=layers[i].posy;
		int16_t w=layers[i].img->w;
		int16_t h=layers[i].img->h;
		
		glBegin(GL_QUADS);
		glVertex4i(
				x,y,
				0,0
			);
		
		glVertex4i(
			 x+w, y,
			 0+w, 0
			);
		
		glVertex4i(
			 x+w, y+h,
			 0+w ,0+h);
		
		glVertex4i(
			x, y+h,
			0, 0+h);
		glEnd();	
		
	
		// this is also very important
		
	}
	
	
	uint16_t width= 128;
	uint16_t height =128;
		
	uint8_t pixels[width * height];

	glReadPixels(0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, pixels);
	
	
	if (layers[0].img && gif && rendergif){
		
		if(framenr < 64){
			
			for (int row=0; row <height ;row++){
				memcpy(gif->frame+row*width, pixels+width*height - (row+1)*width, width);
			}
		}
		
		framenr++;
		
		if (framenr == 64){
			ge_close_gif(gif);
			gif=0;
			rendergif=0;
		}
		
		if(framenr < 64){
			ge_add_frame(gif, 4);
		}
		
	}
	}
	te_gtkgl_swap(TE_GTKGL(gl));
	
	
	return TRUE;

}

static void
refresh(GtkWidget *bt, gpointer ud) {
	
		
	
	
	
	n_layers=0;
	
	framenr=0;
	
	

	for (uint8_t i =0;i<8;i++){
		
		VarMapPair vmp = {
			"img",
			63
		};
		
		layers[i].var_map[63]=vmp;
		
		layers[i].n_instr=0;
		layers[i].instr_p=0;
			
		layers[i].posx=0;
		layers[i].posy=0;
		layers[i].shiftx=0;
		layers[i].shifty=0;
		
		VarMapPair jumpinstr[64];
		uint8_t n_jumpinstr=0;
		
		const char* labels[64];
		uint16_t ips[64];
		uint8_t n_labels=0;
		
		GtkTextIter start_iter;
		GtkTextIter end_iter;
		
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textViews[i]));
		
		
		
		
		
		
		
		
		gtk_text_buffer_get_start_iter(buffer, &start_iter);
		gtk_text_buffer_get_end_iter(  buffer, &end_iter);
		char* str=gtk_text_buffer_get_text(buffer,&start_iter,&end_iter,0);
		if(!strlen(str))return;

		n_layers++;	
			//char* nextline=str;
		char* line=str;
		char* nextline=strchr(line,'\n');
		if(nextline) *(nextline) = 0;
		
		
		while (line){
			
			char* f=strtok(line, " ");
			char* a1=strtok(0, " ");
			char* a2=strtok(0, " ");
			void (*func)(layer*,short arg1,short arg2);
				
			
			printf("%s %s %s\n",f,a1,a2);	
			line=nextline;
			if (line){
				line++;
				nextline=strchr(line,'\n');
				if(nextline) *(nextline) = 0;
			}
			if (!f)continue;
			if (f[0]=='.'){
				labels[n_labels]=f+1;
				ips[n_labels]=layers[i].n_instr;
				n_labels++;
				
				
				for (int l=0;l<n_jumpinstr;l++){
					
					if (! strcmp(f+1,jumpinstr[l].key)){
						
						layers[i].instr[jumpinstr[l].val].arg1=layers[i].n_instr;
						
					}
				}				
				
				
				continue;
			}else if( !(func=getFunc(f)) ) {
				continue;
			}
			
			
			
			
			int16_t arg1;
			int16_t arg2;
			
			
			if ( isdigit(a1[0]) || a1[0]=='-'  ){
				arg1=atoi(a1);
			}
			else if ( a1[0]=='$' ){
				arg1=atoi(a1+1);
			}
			else if (a1[0]=='.') {
				char found=0;
				printf("ll_\n");
				for (int l=0;l<n_labels;l++){
					if (! strcmp(a1+1,labels[l])){
						arg1=ips[l];
						found=1;
						break;
					}
				}
				if (!found){
					VarMapPair ssp={a1+1,layers[i].n_instr};
					jumpinstr[n_jumpinstr++]=ssp;
				}
			}else {
				if ( ( arg1=getVar(layers[i].var_map,layers[i].var_map_size , a1)) <0 ){
					arg1=getVar(var_map,var_map_size,a1);
				}
				
			}
			
			
			
			if (!a2){
				arg2=0;
			}
			else if ( isdigit(*a2) || a2[0]=='-' ){
				arg2=atoi(a2);
			}
			else if ( a2[0]=='$' ){
				arg2=atoi(a2+1);
			}else {
				if ( ( arg2=getVar(layers[i].var_map,layers[i].var_map_size,a2)) <0 ){
					arg2=getVar(var_map,var_map_size,a2);
				}
			}
			
			
			
			instruction in={
				.func=func,
				.arg1= arg1,
				.arg2= arg2,
				.a1_type=a1[0]=='$',
				.a2_type=a2 ? a2[0]=='$': 0
			};
			
			
			
			layers[i].instr[layers[i].n_instr]=in;

			layers[i].n_instr++;
			
				
			f=strtok(0, " ");
			
				
		}
		printf("\n");

			
		
		glGenTextures(1,&(layers[i].texture));
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB ,layers[i].texture);
			
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_R, GL_REPEAT);
			
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB ,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB ,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		
	/*	
			gtk_text_buffer_create_tag(buffer, "gray_bg", 
      "background", "darkblue", NULL); 
		
		gtk_text_buffer_apply_tag_by_name(buffer, "gray_bg", 
        &start_iter, &end_iter);
	
		*/
		
	}
	
	printf("var_map_size: %i\n",var_map_size);
}

static unsigned int compileShader(unsigned int type, char* src){
	unsigned int id= glCreateShader(type);
	
	glShaderSource(id,1,&src,0);
	glCompileShader(id);

	int result;
	glGetShaderiv(id,GL_COMPILE_STATUS, &result);
	
	if (!result || !id){
		
		int length;
		glGetShaderiv(id,GL_INFO_LOG_LENGTH,&length);
		char* message= (char*)alloca(length*sizeof(char));
		
		glGetShaderInfoLog(id,length,&length,message);
		printf("error: %s\n",message);
		glDeleteShader(id);
		return 0;
	}
	
	return id;
}



static unsigned int createShader(){
	unsigned int program = glCreateProgram();
	printf("%d\n",program);
	char* vs;
	char* fs;
	
	{
	strcpy(path_from_res,"basic.vs" );
	printf("ptr: %s\n", path_to_res);
	FILE *fp1=fopen(path_to_res,"r");//open the input file
	fseek(fp1, 0L, SEEK_END);
	uint32_t sz = ftell(fp1);
	fseek(fp1, 0L, SEEK_SET);
	vs = (char*)calloc(sz,sizeof(char));
	fread( vs, sizeof(char), sz,fp1 );
	fclose(fp1);
	}
	{
	strcpy(path_from_res,"basic.fs" );
	FILE *fp2=fopen(path_to_res,"r");//open the input file
	fseek(fp2, 0L, SEEK_END);
	uint32_t sz = ftell(fp2);
	fs = (char*)calloc(sz,sizeof(char));
	fseek(fp2, 0L, SEEK_SET);
	fread( fs, sizeof(char), sz,fp2 );
	fclose(fp2);
	
	*path_from_res = 0; 
		
	}

	unsigned int vsid = compileShader(GL_VERTEX_SHADER,vs);
	printf("vertex shader compiled, id=%d\n",vsid);
	
	unsigned int fsid = compileShader(GL_FRAGMENT_SHADER,fs);
	printf("fragment shader compiled, id=%d\n",fsid);
	

	glAttachShader(program,vsid);
	glAttachShader(program,fsid);

	glLinkProgram(program);
	glValidateProgram(program);

	glDeleteShader(vsid);
	glDeleteShader(fsid);

	free (vs);
	free (fs);

	return program;

}
/*
layerlist* loadimages(const char* name){
	char namebuffer[20];
	
	sprintf(namebuffer,"%s%04d.bmp",name,0);
	layerlist* ll =(layerlist*) malloc(sizeof(layerlist));
	layerlist* prev = ll;
	SDL_Surface* img= SDL_LoadBMP(namebuffer);
	prev->img=img;
	prev->prev=0;
	prev->n_instr=0;
	prev->wait=4;
	sprintf(namebuffer,"%s%04d.bmp",name,1);
	ll->next=0;
	for (uint8_t i =2;
		 
		img = SDL_LoadBMP(namebuffer);
		sprintf(namebuffer,"%s%04d.bmp",name,i++)
	){
		layerlist* curlayer=(layerlist*) malloc(sizeof(layerlist));
		curlayer->img=img;
		curlayer->next=0;
		curlayer->n_instr=0;
		curlayer->wait=4;
		curlayer->prev=prev;
		prev->next=curlayer;
		prev=curlayer;
		
		printf("%s\n",namebuffer);
	}
	printf("images loaded\n");
	
	return ll;
}

*/


int main(int argc, char *argv[]) {
	
	
	printf("%s\n",  argv[0]); 
	strcpy(path_to_res,argv[0]);
	strcpy(path_to_res+strlen(argv[0]) -4, "../res/");
	path_from_res = path_to_res + strlen(path_to_res);
	

	
	
	

	
	
	printf("mod:%d\n",(-7%5)+5);
    GtkWidget *cnt, *gl, *bt1,*bt2,*bt3,*bt4,*bt5,*bt6,*view1,*view2,*view3,*slider1,*slider2;
    
	
    
    // initialize GTK+
    gtk_init(&argc, &argv);
	
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(gtk_main_quit), 0);

    // create the OpenGL widget
    g_gl_wid = gl = te_gtkgl_new();

    bt1 = gtk_button_new_with_label("(>)");
	bt2 = gtk_button_new_with_label("||");
	bt3 = gtk_button_new_with_label("load");
	bt4 = gtk_button_new_with_label("save");
	bt5 = gtk_button_new_with_label("^images");
	bt6 = gtk_button_new_with_label("\/gif");
	//slider1 = gtk_slider_new();
	//slider2 = gtk_slider_new();
	
    g_signal_connect(G_OBJECT(bt1), "clicked", G_CALLBACK(refresh), (gpointer)&cnt);
	g_signal_connect(G_OBJECT(bt2), "clicked", G_CALLBACK(pause_func), (gpointer)&cnt);
	g_signal_connect(G_OBJECT(bt3), "clicked", G_CALLBACK(load_func), (gpointer)&cnt);
	g_signal_connect(G_OBJECT(bt4), "clicked", G_CALLBACK(save_func), (gpointer)&cnt);
	g_signal_connect(G_OBJECT(bt5), "clicked", G_CALLBACK(load_images_func), (gpointer)&cnt);
	g_signal_connect(G_OBJECT(bt6), "clicked", G_CALLBACK(export_gif), (gpointer)&cnt);
	

    // set a callback that will stop the timer from drawing
    g_signal_connect(G_OBJECT(gl), "destroy", G_CALLBACK(destroy_the_gl), 0);

    gtk_widget_add_events(gl,  GDK_ALL_EVENTS_MASK);
    g_signal_connect(G_OBJECT(gl), "button-press-event", G_CALLBACK(on_clicked), 0);
	

    // our layout
    cnt = gtk_grid_new();
	
	gtk_grid_attach(GTK_GRID(cnt), gl,    0,  0, 7, 6);
	
    gtk_grid_attach(GTK_GRID(cnt), bt1,   8,  0, 1, 1);
	gtk_grid_attach(GTK_GRID(cnt), bt2,   8,  1, 1, 1);
	gtk_grid_attach(GTK_GRID(cnt), bt3,   8,  2, 1, 1);
	gtk_grid_attach(GTK_GRID(cnt), bt4,   8,  3, 1, 1);
	gtk_grid_attach(GTK_GRID(cnt), bt5,   8,  4, 1, 1);
	gtk_grid_attach(GTK_GRID(cnt), bt6,   8,  5, 1, 1);
	
	GtkWidget* frames[8];
	GtkWidget* scrollpanes[9];

	for (uint8_t i;i<8;i++){
		char namebuffer[8]="Layer__";
		namebuffer[6]=48+i;
		frames[i] =gtk_frame_new(namebuffer);
		scrollpanes[i] = gtk_scrolled_window_new(NULL, NULL);
		textViews[i] =gtk_text_view_new();
		
		gtk_container_add(GTK_CONTAINER(frames[i]),scrollpanes[i]);
		gtk_container_add(GTK_CONTAINER(scrollpanes[i]),textViews[i]);
		
		
		GtkTextIter iter;
		gtk_widget_set_size_request(scrollpanes[i], 140, 400);
		
		
		gtk_grid_attach(GTK_GRID(cnt), frames[i], i,  6, 1, 1);
		
	}
	
	// create the window and set it to quit application when closed
    
	
    // create menu
	scrollpanes[8] = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrollpanes[8], 140, 400);
		
	imglist_widget = gtk_list_box_new ();
	
	gtk_container_add(GTK_CONTAINER(scrollpanes[8]), imglist_widget);
    
	
	
	gtk_grid_attach(GTK_GRID(cnt), scrollpanes[8],   7,  0, 1, 6);
	
	
	
	
    // bureaucracy and show things on screen
	
	//gtk_container_add(GTK_CONTAINER(win),GTK_CONTAINER(menubar) );
    gtk_container_add(GTK_CONTAINER(win), cnt);
    gtk_widget_set_size_request(gl, 1024, 512);
    gtk_widget_show_all(win);

    g_timeout_add_full(1000, 10, draw_the_gl, 0, 0);
	
    te_gtkgl_make_current(TE_GTKGL(gl));
		glEnable(GL_TEXTURE_RECTANGLE_ARB);
		shader=createShader();
		
		

	glUseProgram(shader);
/*
	glUniform1uiv( glGetUniformLocation(shader,"palette") ,16,
	   colortable
	);
*/		
	
	
	gtk_main();
    return 0;
	
	

	
	

}


/*
 * 
 * mov 20 32
set 5 0
.l1
set 0 run00
.l2
img $0

//cmp $0 8
//jeq 8

//cmp $0 4
//jne 32

cmp $5 1
jne .dontjump

mov 0 $8
drw 2
mov 0 $8

cmp $8 -4
jne .l123
mov 0 -5

.l123

drw 2


add 0 1
img $0
drw 3

set 0 4
set 5 0
img $0

mov 0 1
drw 5



cmp $8 -2
jeq .a3
add 9 1
mov 0 -1
drw 2
mov 0 -2
drw 2

set 0 loop00

img $0
drw 2

.a2
img $0
drw 1
add 0 3
cmp $0 loop06
add 0 -2
img $0
jle .a2

mov 0 3
drw 2
add 0 1
mov 0 4
drw 3

.a3

mov 0 3
drw 1

set 0 run00
img $0

cmp $8 -4
jne .dontjump

mov 0 3
drw 2
add 0 1
img $0

mov 0 2
drw 1



.dontjump
drw 3
add 0 1
cmp $0 21 
jle .l2
jmp .l1
 * 
 * 
 * 
 * 
img bg
mov 0 -1
.start
drw 2

jmp .start
---------------------

---------------------
img midground
mov 0 -6
.start
drw 2

shf 1
jmp .start
-------------------

-------------------
img mg
mov 0 -6
.start
drw 1

shf 1
jmp .start
-------------------

-------------------
img wall
mov 0 -7
.start
drw 1

shf 2 0
jmp .start
---------------------

---------------------
mov 20 42
set 7 20
img hole
set 8 -2



.start
cmp $8 -2
jeq .s1

set 8 -2
set 9 hole
mov 228 10
add 7 228

jmp .l1

.s1

set 8 -4
set 9 car
mov 228 -10
add 7 228

.l1
img $9
drw 1
mov $8 0
add 7 $8
cmp $7 -100
jeq .start

cmp $8 -2
jne .l2

cmp $7 40
jne .l1
set 5 1
jmp .l1


.l2

cmp $7 60
jne .l1
set 5 1
jmp .l1
------------------

------------------
mov 20 32
set 5 0
.l1
set 0 run
.l2
img $0

//cmp $0 8
//jeq 8

//cmp $0 4
//jne 32

cmp $5 1
jne .dontjump

mov 0 $8
drw 2
mov 0 $8

cmp $8 -4
jne .l123
mov 0 -5

.l123

drw 2


add 0 1
img $0
drw 3

set 0 4
set 5 0
img $0

mov 0 1
drw 5



cmp $8 -2
jeq .a3
add 9 1
mov 0 -1
drw 2
mov 0 -2
drw 2

set 0 loop

img $0
drw 2

.a2
img $0
drw 1
add 0 3
cmp $0 30
add 0 -2
img $0
jlt .a2

mov 0 3
drw 2
add 0 1
mov 0 4
drw 3

.a3

mov 0 3
drw 1

set 0 run
img $0

cmp $8 -4
jne .dontjump

mov 0 3
drw 2
add 0 1
img $0

mov 0 2
drw 1



.dontjump
drw 3
add 0 1
cmp run_end $0 
jgt .l2
jmp .l1
------------------

------------------
img ari
mov 24 10
set 10 ari
set 4 1
.start
set 1 0
.straight
add 1 1
set 10 ari
img $10
drw 6
add 10 1
img $10
drw 6
cmp $1 5
jne .straight
cmp $4 1
set 3 0
jeq .down
set 2 0
.up
add 2 1
set 10 ari_up
img $10

drw 3
mov -1 -1
add 10 1
img $10
drw 3
mov 0 -1
cmp $2 6
jne .up
set 4 1
jmp .start
.down
add 3 1
img ari_down
drw 5
mov 1 1
drw 3
mov 0 1
drw 3
cmp $3 6
jne .down
set 4 0
jmp .startimg ari
mov 24 23
set 10 ari
set 4 0
.start
set 1 0
.straight
add 1 1
set 10 ari
img $10
drw 6
add 10 1
img $10
drw 6
cmp $1 5
jne .straight
cmp $4 1
set 3 0
jeq .down
set 2 0
.up
add 2 1
set 10 ari_up
img $10

drw 3
mov -1 -1
add 10 1
img $10
drw 3
mov 0 -1
cmp $2 6
jne .up
set 4 1
jmp .start
.down
add 3 1
img ari_down
drw 5
mov 1 1
drw 3
mov 0 1
drw 3
cmp $3 6
jne .down
set 4 0
jmp .start
----------------------

----------------------
img front
mov 0 25
.start
drw 1

shf 3 0
jmp .start
 * 
 * 
 * 
 * 
*/
