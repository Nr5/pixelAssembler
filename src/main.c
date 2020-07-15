#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
//#include <gtksourceview/gtksourceview.h>
#include <gdk/gdk.h>
#include "tegtkgl.h"
#include <GL/gl.h>
#include <math.h>
#include <SDL/SDL.h>
#include <FreeImage.h>
#include "gifenc.h"
#include <gmodule.h>
#include <gdk/gdkkeysyms.h>
#define IS_BIG_ENDIAN (!*(unsigned char *)&(uint16_t){1})


/*-------------
 ** Render **
-------------*/
static gboolean do_the_gl = TRUE;
uint16_t width_gif   ;
uint16_t height_gif  ;
uint16_t frame_count ;
uint16_t width 		= 128;
uint16_t height 	= 128;
float 	 scale 		= 1.f;

uint8_t  rendergif 		= 0;
uint8_t  *gif_palette 	= 0;
uint32_t colortable 	= 0;

char     image_name_buffer[65535];
char*    image_name_pointers[128];
uint16_t image_name_buffer_length;

char     name_buf2[65535];
ge_GIF * gif;


/*------------
   ** UI **
------------*/
static GtkWidget *g_gl_wid = 0;
GtkWidget *win;
GtkWidget* colorbuttons[256];
GtkWidget* textViews[8];
GtkWidget* menubar;
GtkWidget* imglist_widget;

uint8_t transparent_color_index;   
static unsigned int shader;
unsigned char time_0=0;
char paused = 0;
char p2=1;



/*--------------
  ** Logic ** 
--------------*/
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

typedef struct VarMapPair{
	const char* key;
	int16_t val;
}VarMapPair;

uint16_t framenr=0;
VarMapPair var_map[256];
var_map_size=0;

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

int16_t getVar(VarMapPair *var_map,uint8_t var_map_size,const char* name) {
	for (int i=0;i<var_map_size;i++){
		if (!strcmp(var_map[i].key ,name)){
			return var_map[i].val;
		}
	}
	return -1;
}

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

layer layers[16];
char path_to_res[0];
char* path_from_res;

int (*getFunc(const char* name))(layer*,short, short) {
	for (int i=0;i<instr_map_size;i++){
		if (!strcmp(instr_map[i].key ,name)){
			return instr_map[i].val;
		}
	}
	return 0;
}

static void pause_func(GtkWidget *bt, gpointer ud){
		paused=!paused;
		
}
static void load_images_func_helper(const char* name,gpointer _);
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
//  TODO the end of this function is really messy tidy up
	name_buf2[0]=0;
	fread (&n_images, 1,1,fp);
	fread (&image_name_buffer_length,2,1,fp);
	if (image_name_buffer_length) fread (name_buf2,1,image_name_buffer_length,fp);

	char* buf_end = name_buf2 + image_name_buffer_length;


	printf("n_images: %i\n",n_images);
	printf("image_name_buffer: %s\n",name_buf2);
	printf("image_name_buffer_length: %i\n",image_name_buffer_length);
	
	image_name_buffer_length = 0;
	n_images=0;
	for (char* name = name_buf2; name < buf_end; name += strlen(name) + 1 + 4 ){  
	// NOTE the +4 is needed to skip over the suffix (bmp)	because load_images_helper_func splits it off
			printf("%s\n", name);
			load_images_func_helper(name,0);
			printf("next..\n");
			
	}
// TODO this code is a copy from load_images_func() 
// probably needs some refactoring
	if(!colortable && n_images){
		colortable=((uint32_t*)images[0]->format->palette->colors);
		
		glUniform1uiv( glGetUniformLocation(shader,"palette") ,images[0]->format->palette->ncolors,
		(uint32_t*) images[0]->format->palette->colors
		);
		
		gif_palette=malloc(images[0]->format->palette->ncolors *3 );
		
		for (int i=0;i<images[0]->format->palette->ncolors;i++){
			gif_palette[i*3]=images[0]->format->palette->colors[i].r;
			gif_palette[i*3+1]=images[0]->format->palette->colors[i].g;
			gif_palette[i*3+2]=images[0]->format->palette->colors[i].b;
			
			GdkColor color= {0,
					images[0]->format->palette->colors[i].r*256,
					images[0]->format->palette->colors[i].g*256,
					images[0]->format->palette->colors[i].b*256
			};
			gtk_widget_modify_bg ( colorbuttons[i], GTK_STATE_NORMAL, &color);
		}
	}
	
	gtk_widget_show_all(imglist_widget);

	fclose(fp);	
	g_free (filename);
  }
	gtk_widget_destroy (dialog);
		
}

static void load_images_func_helper(const char* name,gpointer _){
	printf("image_name: %s\n",name);
	SDL_Surface* img=SDL_LoadBMP(name);
	
	uint8_t name_length = strlen(name) + 1;
	memcpy(image_name_buffer+image_name_buffer_length, name,name_length);
	image_name_pointers[n_images] = image_name_buffer_length;
	image_name_buffer_length += name_length;
	

	for (const char* name2; name2 = strstr(name,"/"); name = name2+1);
	
	char* tmpname=	strstr(name,".");
	tmpname[0]=0;
	VarMapPair vmp={name,n_images};
		
	var_map[var_map_size++]=vmp;
	
	images[n_images++]=img;
	printf("vmp: %s,%i\n"  ,vmp.key,vmp.val);
	printf("var_map_size: %i\n",var_map_size);
	printf("n_images: %i\n",n_images);
	
	GtkWidget* label= gtk_label_new(name);
	gtk_label_set_xalign (label, 0);
	gtk_container_add(GTK_CONTAINER(imglist_widget), label);
	//tmpname[0]=".";
    
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
			
			GdkColor color= {0,
					images[0]->format->palette->colors[i].r*256,
					images[0]->format->palette->colors[i].g*256,
					images[0]->format->palette->colors[i].b*256
			};
			gtk_widget_modify_bg ( colorbuttons[i], GTK_STATE_NORMAL, &color);
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
	
	

	fwrite(&n_images,1,1,fp );
	fwrite(&image_name_buffer_length,2,1,fp );
	fwrite(&image_name_buffer,1,image_name_buffer_length,fp );
	
	//printf("> %s\n", image_name_buffer + (int) image_name_pointers[i] );

	
	fclose(fp);	
	
	g_free (filename);
  }
	gtk_widget_destroy (dialog);
		
}

static void destroy_the_gl(GtkWidget *wid, gpointer ud) {
    do_the_gl = FALSE;
}

static gboolean export_gif(){
gint res;
	GtkWidget *label_width, *label_height, *label_frames, *content_area, *details_dialog, *spinner_x, *spinner_y, *spinner_f,  *ok_button, *grid;
	details_dialog = gtk_dialog_new_with_buttons("export gif",0 ,0 , 
				  ("_OK"),
                  GTK_RESPONSE_ACCEPT,
                  ("_Cancel"),
                  GTK_RESPONSE_REJECT,
				  0);
		
		
		
	label_width  = gtk_label_new("width:  			");
	label_height = gtk_label_new("height: 			");
	label_frames = gtk_label_new("number of frames: ");
		
	spinner_x = gtk_spin_button_new_with_range(0,65535,1);
	spinner_y = gtk_spin_button_new_with_range(0,65535,1);
	ok_button = gtk_button_new_with_label("Ok");
	spinner_f = gtk_spin_button_new_with_range(0,65535,1);
	grid      = gtk_grid_new();
		
	content_area = gtk_dialog_get_content_area (details_dialog);
		
	gtk_grid_attach(GTK_GRID(grid), label_width  ,   0,  0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), label_height ,   0,  1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), label_frames ,   0,  2, 1, 1);
	
	gtk_grid_attach(GTK_GRID(grid), spinner_x,   1,  0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), spinner_y,   1,  1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), spinner_f,   1,  2, 1, 1);
	
	gtk_container_add(GTK_CONTAINER(content_area),grid);
	
	gtk_widget_show_all(details_dialog);
	
	res = gtk_dialog_run (GTK_DIALOG (details_dialog));
	
	if (res != GTK_RESPONSE_ACCEPT) return;
	
	width_gif    = gtk_spin_button_get_value_as_int(spinner_x);
	height_gif   = gtk_spin_button_get_value_as_int(spinner_y);
	frame_count  = gtk_spin_button_get_value_as_int(spinner_f);


	GtkWidget *dialog;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;

	dialog = gtk_file_chooser_dialog_new ("Save File",
                                      win,
                                      action,
                                      "Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "Save",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);

	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_ACCEPT){
    	char *filename;
    	GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
	
		gtk_file_chooser_set_current_folder (chooser,  path_to_res);
    	filename = gtk_file_chooser_get_filename (chooser);
    
		rendergif=1;
		framenr=0;
		gif=ge_new_gif(
        	filename,        
        	width_gif, height_gif,   
        	gif_palette, 4, 
        	0              
    	);
	
	
		g_free (filename);
  	}
	gtk_widget_destroy (dialog);
	gtk_widget_destroy (details_dialog);
	
}

static gboolean on_clicked(GtkWidget *wid, GdkEvent *ev, gpointer user_data) {
    printf("clicked at %.3fx%.3f with button %d\n",
        ev->button.x, ev->button.y, ev->button.button);
    
	export_gif();
	
	return TRUE;
}

gboolean draw_the_gl(gpointer ud) {
	if (paused) return TRUE; 
    static float s = 0.f;
    GtkWidget *gl = g_gl_wid;
    GdkWindow *wnd;
    te_gtkgl_make_current(TE_GTKGL(gl));
	glClearColor(0, 0, 0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
   	

	glUseProgram(shader);
	if (!do_the_gl)
        return FALSE;


    for (int i=0;i<1+frame_count * rendergif;i++){
		
		if(rendergif){
			glViewport(0, 0, width_gif, height_gif);
		}else{
			glViewport(
					gtk_widget_get_allocated_width(gl)  / 2 - width  * scale / 2, 
					gtk_widget_get_allocated_height(gl) / 2 - height * scale / 2, 
					width * scale,
				   	height * scale
			  );
	
		}
	
		glEnable(GL_TEXTURE_RECTANGLE_ARB);	
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLoadIdentity();
    
		glOrtho(0, width, height, 0, 0, 1.0);
		float modelview[16];
		glMatrixMode(GL_MODELVIEW);
		glGetFloatv(GL_MODELVIEW_MATRIX,modelview);
	
		glUniformMatrix4fv( glGetUniformLocation(shader,"matrix") ,1,0,modelview );
		glUniform1ui( glGetUniformLocation(shader,"gif") ,rendergif );
	
		
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
	
	uint8_t pixels[width_gif * height_gif];

	glReadPixels(0, 0, width_gif, height_gif, GL_RED, GL_UNSIGNED_BYTE, pixels);
	
	if (layers[0].img && gif && rendergif){
		
		if(framenr < frame_count - 1){
			
			for (int row=0; row < height_gif; row++){
				memcpy(gif->frame+row*width_gif, pixels+width_gif*height_gif - (row+1)*width_gif, width_gif);
			}
		}
		
		framenr++;
		
		if (framenr == frame_count - 1){
			ge_close_gif(gif);
			gif=0;
			rendergif=0;
		}
		
		if(framenr < frame_count - 1){
			ge_add_frame(gif, 4);
		}
		
	}
	}
	
	
	
	glUseProgram(0);

	glDisable(GL_TEXTURE_RECTANGLE_ARB);	
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	//glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glLoadIdentity();
	
	glViewport(
			gtk_widget_get_allocated_width(gl)  / 2 - width  * scale / 2 -1, 
			gtk_widget_get_allocated_height(gl) / 2 - height * scale / 2 -1, 
			width * scale +2,
			height * scale +2
		  );

	glColor4f(0,0,1,1);
	glBegin(GL_LINE_LOOP);
	glVertex3f( .01,   	.01, 	 0);
	glVertex3f( .01,    height,  0);
	glVertex3f( width, 	height,  0);
	glVertex3f( width, 	.01,     0);
	glEnd();

	glUseProgram(shader);
	
	te_gtkgl_swap(TE_GTKGL(gl));
	return TRUE;

}

static void refresh(GtkWidget *bt, gpointer ud) {
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

gboolean zoom (GtkWidget *bt, GdkEvent  *event, gpointer ud) {
	if (ud){
		scale *= 1-((GdkEventScroll*) event)->delta_y/10.f;
		return 1;
	}
	if ( ((GdkEventKey*) event)->state & GDK_CONTROL_MASK ){
		if ( ((GdkEventKey*) event)->keyval == GDK_KEY_plus  ){
			scale *= 11.f / 10;
			return 1;
		}
		if ( ((GdkEventKey*) event)->keyval == GDK_KEY_minus  ){
			scale *= 10.f / 11;
			return 1;
		}
	}
	return 0;

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

void spinner_value_changed(GtkWidget *spinner, gpointer ud){
	*((uint16_t*)ud) = gtk_spin_button_get_value_as_int(spinner);
}

static gboolean select_transparent_color(GtkWidget *wid, GdkEvent *ev, gpointer user_data) {
	transparent_color_index = (uint8_t) user_data;
	glUniform1ui( glGetUniformLocation(shader,"transparent_index") ,(uint8_t) user_data);
	return 1;
}

int main(int argc, char *argv[]) {
	
	
	printf("%s\n",  argv[0]); 
	strcpy(path_to_res,argv[0]);
	strcpy(path_to_res+strlen(argv[0]) -4, "../res/");
	path_from_res = path_to_res + strlen(path_to_res);
	
	
	printf("mod:%d\n",(-7%5)+5);
    GtkWidget *palette_widget, *palette_grid, 
			  *cnt, *grid_down, *grid_buttons,
			  *gl, 
			  *bt1,*bt2,*bt3,*bt4,*bt5,*bt6,
			  *label_width, *label_height,
			  *spinner_x, *spinner_y,
			  *view1,*view2,*view3
			  ;
    
    gtk_init(&argc, &argv);
	
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(gtk_main_quit), 0);

    g_gl_wid = gl = te_gtkgl_new();


	palette_widget=gtk_window_new(0);
		

    bt1 = gtk_button_new_with_label("(>)");
	bt2 = gtk_button_new_with_label("||");
	bt3 = gtk_button_new_with_label("load");
	bt4 = gtk_button_new_with_label("save");
	bt5 = gtk_button_new_with_label("import images");
	bt6 = gtk_button_new_with_label("export as gif");
	
	label_width  = gtk_label_new("width:");
	label_height = gtk_label_new("height:");
    
	spinner_x = gtk_spin_button_new_with_range(0,65535,1);
	spinner_y = gtk_spin_button_new_with_range(0,65535,1);
	
	g_signal_connect(G_OBJECT(spinner_x), "value_changed", G_CALLBACK(spinner_value_changed), (gpointer)&width);
	g_signal_connect(G_OBJECT(spinner_y), "value_changed", G_CALLBACK(spinner_value_changed), (gpointer)&height);
	
	
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
    
	gtk_widget_add_events(win, GDK_KEY_PRESS_MASK);

	g_signal_connect(G_OBJECT(gl), "scroll-event",    G_CALLBACK(zoom), (gpointer) 1 );
    g_signal_connect(G_OBJECT(win), "key_press_event", G_CALLBACK(zoom), (gpointer) 0 );

    // our layout
    cnt = gtk_grid_new();
	palette_grid = gtk_grid_new();
	grid_down = gtk_grid_new();
	grid_buttons = gtk_grid_new();



	gtk_widget_set_size_request(palette_grid, 140, 140);
    
	
	for (int i = 0; i< 256; i++ ){
		
		colorbuttons[i]=gtk_event_box_new();
    	gtk_widget_add_events(colorbuttons[i],  GDK_BUTTON_PRESS_MASK);

			
	    g_signal_connect(G_OBJECT(colorbuttons[i]), "button-press-event", G_CALLBACK(select_transparent_color),(gpointer)i);
			
		gtk_widget_set_size_request(colorbuttons[i], 16, 16);
		gtk_grid_attach(GTK_GRID(palette_grid), colorbuttons[i],   i%16,  i/16, 1, 1);
		
		
	}
	
	GdkColor color= {0, 65535, 0, 0};	
	gtk_widget_modify_bg ( colorbuttons[0], GTK_STATE_NORMAL, &color);

	GdkColor color2={0, 0, 65535, 0};	
	gtk_widget_modify_bg ( colorbuttons[1], GTK_STATE_NORMAL, &color2);

	GdkColor color3={0, 0, 0, 65535};	
	gtk_widget_modify_bg ( colorbuttons[2], GTK_STATE_NORMAL, &color3);
	
	gtk_grid_attach(GTK_GRID(cnt), gl,    0,  0, 1, 2);
    gtk_grid_attach(GTK_GRID(cnt), palette_grid,   2,  0, 1, 1);
	
  	gtk_grid_attach(GTK_GRID(grid_buttons), bt1,   0,  0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_buttons), bt2,   1,  0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_buttons), bt3,   0,  1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_buttons), bt4,   0,  2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_buttons), bt5,   1,  1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_buttons), bt6,   1,  2, 1, 1);
	
	gtk_spin_button_set_value(spinner_x,width );
	gtk_spin_button_set_value(spinner_y,height);
  	
	gtk_grid_attach(GTK_GRID(grid_buttons), label_width,    0,  3, 1, 1);
  	gtk_grid_attach(GTK_GRID(grid_buttons), spinner_x,      1,  3, 1, 1);
  	gtk_grid_attach(GTK_GRID(grid_buttons), label_height,   0,  4, 1, 1);
  	gtk_grid_attach(GTK_GRID(grid_buttons), spinner_y,      1,  4, 1, 1);
    


	gtk_grid_attach(GTK_GRID(cnt), grid_buttons,   2,  1, 1, 1);
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
		gtk_widget_set_size_request(scrollpanes[i], 0, 400);
		gtk_widget_set_hexpand(scrollpanes[i],1);
		gtk_widget_set_vexpand(scrollpanes[i],1);
		
		gtk_grid_attach(GTK_GRID(grid_down), frames[i], i,  0, 1, 1);
			
	}
	
	gtk_grid_attach(GTK_GRID(cnt),grid_down,0,2,3,1);

	// create the window and set it to quit application when closed
    
	
    // create menu
	scrollpanes[8] = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrollpanes[8], 140, 400);
		
	imglist_widget = gtk_list_box_new ();
	
	gtk_container_add(GTK_CONTAINER(scrollpanes[8]), imglist_widget);
    
	
	
	gtk_grid_attach(GTK_GRID(cnt), scrollpanes[8],   1,  0, 1, 2);
	
	
	
	
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

