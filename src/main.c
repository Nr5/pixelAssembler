#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <gtksourceview/gtksourceview.h>
#include <gdk/gdk.h>
#include <GL/glew.h>
#include "tegtkgl.h"
#include <GL/gl.h>
#include <math.h>
#include <SDL/SDL.h>
#include <FreeImage.h>
#include "./gif/gifenc.h"
#include "./gif/gifdec.h"
#include <gmodule.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "pxa_logic.h"

#define IS_BIG_ENDIAN (!*(unsigned char *)&(uint16_t){1})

/*-------------
 ** Render **
-------------*/

static gboolean do_the_gl = TRUE;
uint16_t width_gif;
uint16_t height_gif;
uint16_t frame_count;
uint16_t width 		= 128;
uint16_t height 	= 128;
float 	 scale 		= 1.f;
uint8_t  scale_gif  = 1;

uint8_t   rendergif 	= 0;
uint8_t   *gif_palette 	= 0;
uint32_t* colortable 	= 0;

char     image_name_buffer[65536];
//char*    image_name_pointers[256];
uint16_t image_name_buffer_length;

uint8_t framerate = 24;
uint8_t framerate_changed = 0;

char     name_buf2[65536];
ge_GIF* gif;
gd_GIF* gif_dec;

/*-------------
   ** UI ** 
-------------*/

enum flags{
 step 	= 1<<0,
 debug 	= 1<<1,
 paused	= 1<<2
};
uint8_t state_flags=0;


uint16_t prev_ips[8];
uint16_t lines_of_ips[0x10000][8];

GtkTextTag* ip_tags[8];	
GdkRGBA textcolor =(GdkRGBA)	{
			0xffff,
			0xffff,
			0xffff,
			0xffff
	};

static GtkWidget *g_gl_wid = 0;
GtkWidget *win;
GtkWidget* colorbuttons[256];
GtkWidget* textViews[8];
GtkWidget* menubar;
GtkWidget* imglist_widget;
GtkWidget* show_regs_widgets[7][8] ;

uint8_t transparent_color_index;   
static unsigned int shader;

char path_to_res[64];
char* path_from_res;

static void load_images_func_helper(const char* name,gpointer _){
	(void)(_);
	SDL_Surface* img=SDL_LoadBMP(name);
	
	uint8_t name_length = strlen(name) + 1;
	memcpy(image_name_buffer+image_name_buffer_length, name,name_length);
	image_name_buffer_length += name_length;
	
	for (const char* name2; (name2 = strstr(name,"/")); name = name2+1);
			
	char* tmpname = strstr(name,".");
	tmpname[0]    = 0;
	
	char* vmp_key = malloc(strlen(name)+1); // TODO free memory
	memcpy(vmp_key,name,strlen(name)+1);
	tmpname[0]='.';

	VarMapPair vmp={vmp_key,n_images};
		
	var_map[var_map_size++]=vmp;
	
	uint8_t* pixelarray = malloc(img->w * img->h); // TODO free memory
	memcpy(pixelarray, img->pixels, img->w * img->h);

	images[n_images] = (px_image) {
		.width  = img->w,
		.height = img->h,
		.pixels = pixelarray
	};

	memcpy(images[n_images++].palette, img->format->palette->colors, img->format->palette->ncolors*4);

		
	GtkWidget* label= gtk_label_new(vmp_key);
	gtk_label_set_xalign (GTK_LABEL(label), 0);
	gtk_container_add(GTK_CONTAINER(imglist_widget), label);
	//tmpname[0]=".";
			
}

static void toggle_flag_func(GtkWidget *bt, gpointer ud){
	(void)(bt);
		
	if (state_flags & (uint64_t)ud){
		state_flags &= ~ ((uint64_t)ud);
	}
	else {	
		state_flags |=  (uint64_t)ud;
	}


}

static void step_func(GtkWidget *bt, gpointer ud){
	(void)(bt);
	(void)(ud);

	state_flags &= ~paused;
	state_flags |= step;
	


}
static void load_func(GtkWidget *bt, gpointer ud){
	(void)(bt);
	(void)(ud);	
	GtkWidget *dialog;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
	gint res;

	dialog = gtk_file_chooser_dialog_new ("Open File",
    	                                  GTK_WINDOW(win),
        	                              action,
            	                          "Cancel",
                	                      GTK_RESPONSE_CANCEL,
                    	                  "Open",
                        	              GTK_RESPONSE_ACCEPT,
                            	          NULL);

	GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
    
	gtk_file_chooser_set_current_folder (chooser, path_to_res);
	
	GtkFileFilter * filter =gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter,"*.pxa");
	gtk_file_filter_set_name (filter,".pxa files ");
	gtk_file_chooser_add_filter (chooser, filter);
	
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_ACCEPT)  	{
    	const char*  filename = gtk_file_chooser_get_filename (chooser);
    	uint16_t header[8];
		FILE* fp = fopen(filename,"r");
			
		fread(header,2,8,fp);
	
		for (uint8_t i=0;i<8;i++){
	
			char* strbuf = malloc(header[i]); 
			fread (strbuf,1,header[i],fp);
		
		
			GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textViews[i]));
			gtk_text_buffer_set_text ( buffer, strbuf, header[i] );
			
			free (strbuf);
		}
//  TODO the end of this function is really messy tidy up
		name_buf2[0]=0;
		fread (&n_images, 1,1,fp);
		fread (&image_name_buffer_length,2,1,fp);
		if (image_name_buffer_length) fread (name_buf2,1,image_name_buffer_length,fp);

		char* buf_end = name_buf2 + image_name_buffer_length;

	
		image_name_buffer_length = 0;
		n_images=0;
		

		for (char* name = name_buf2; name < buf_end; name += strlen(name) + 1 ){  
// NOTE the +4 is needed to skip over the suffix (bmp)	because load_images_helper_func splits it off
// will cause problems on suffixes longer than 3 characters
			char* filename_end = filename + strlen(filename);
			for (; *filename_end != '/'; filename_end --);		
			filename_end--;	
			for(name = name; *name == '.'; name+=3){
				for (; *filename_end != '/'; filename_end --);		
				filename_end --;
			}
			
			//filename_end[2]=0;
			//char strbuf[128];
			


			memcpy(image_name_buffer + image_name_buffer_length, filename, (filename_end-filename) + 2);
			memcpy(image_name_buffer + image_name_buffer_length + (filename_end-filename) + 2 , name, strlen(name) + 1);
			
			load_images_func_helper(image_name_buffer + image_name_buffer_length,0);

		}
// TODO this code is a copy from load_images_func() 
// probably needs some refactoring
		printf("n_images: %i\n", n_images);
		
		if(!colortable && n_images){
			colortable=((uint32_t*)images[0].palette);
		
			glUniform1uiv( glGetUniformLocation(shader,"palette") , 256 ,//  images[0]->format->palette->ncolors,
			(uint32_t*) images[0].palette
			);
		
			gif_palette=malloc(256 *3 );
		
			for (int i=0;i<256;i++){
				gif_palette[i*3]  =images[0].palette[i].r;
				gif_palette[i*3+1]=images[0].palette[i].g;
				gif_palette[i*3+2]=images[0].palette[i].b;
			
				GdkRGBA color= {
					images[0].palette[i].r / 255.f,
					images[0].palette[i].g / 255.f,
					images[0].palette[i].b / 255.f,
									255	   / 255.f
				};
			
				gtk_widget_override_background_color ( colorbuttons[i], 0, &color);
			}
		}
	
		gtk_widget_show_all(imglist_widget);

		fclose(fp);	
		g_free ((gpointer)filename);
	}

	gtk_widget_destroy (dialog);
		
}

static void load_images_func(GtkWidget *bt, gpointer ud){
	(void)(bt);
	(void)(ud);
	GtkWidget *dialog;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
	gint res;

	dialog = gtk_file_chooser_dialog_new ("Open Files",
    	                              		GTK_WINDOW(win),
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
				    (GFunc)	load_images_func_helper,
					0);
		
		
		gtk_widget_show_all(imglist_widget);
		//g_free (filename);
	}
		gtk_widget_destroy (dialog);
		if(res== GTK_RESPONSE_CANCEL)return;		
			
	if(!colortable){
		colortable=((uint32_t*)images[0].palette);
		
		glUniform1uiv( glGetUniformLocation(shader,"palette") , 256 ,//  images[0]->format->palette->ncolors,
		(uint32_t*) images[0].palette
		);
		
		gif_palette=malloc(256 *3 );  // TODO maybe rather stack allocate
		
		for (int i=0;i<256;i++){
			gif_palette[i*3]  =images[0].palette[i].r;
			gif_palette[i*3+1]=images[0].palette[i].g;
			gif_palette[i*3+2]=images[0].palette[i].b;
			
			GdkColor color= {
					0 ,
					images[0].palette[i].r * 0x0101,
					images[0].palette[i].g * 0x0101,
					images[0].palette[i].b * 0x0101,
			};
			
			gtk_widget_modify_bg ( colorbuttons[i], 0, &color);
		}
	}


}

static void save_func(GtkWidget *bt, gpointer ud){
	(void)(bt);
	(void)(ud);
	GtkWidget *dialog;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
	gint res;

	dialog = gtk_file_chooser_dialog_new ("Save File",
    	                              GTK_WINDOW(win),
                                      action,
                                      "Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "Save",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);

	GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);

	gtk_file_chooser_set_current_folder (chooser, path_to_res);

	GtkFileFilter * filter =gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter,"*.pxa");
	gtk_file_filter_set_name (filter,".pxa files ");
	gtk_file_chooser_add_filter (chooser, filter);
	
	gtk_file_chooser_set_current_folder (chooser, path_to_res);
	
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_ACCEPT)  {
			char *filename;
			filename = gtk_file_chooser_get_filename (chooser);
			
			uint16_t header[8];
			FILE* fp = fopen(filename,"w");
			for (uint8_t i=0;i<8;i++){
				GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textViews[i]));
			
				header[i]=gtk_text_buffer_get_char_count (buffer);
			
				
			}
			fwrite(header,2,8,fp);
			
			for (uint8_t i=0;i<8;i++){
				
				GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textViews[i]));
				
				GtkTextIter start_iter;
				GtkTextIter end_iter;
				gtk_text_buffer_get_start_iter(buffer,&start_iter);
				gtk_text_buffer_get_end_iter(buffer,&end_iter);
								  
				const char* text=	gtk_text_buffer_get_text (buffer,&start_iter,&end_iter, 1);
				fputs (text,fp);
			}
			
			fwrite(&n_images,1,1,fp );
			
			//fwrite(&image_name_buffer_length,2,1,fp );
			
			char* tmp_img_name_buffer = image_name_buffer;
			char* filename_end = filename + strlen (filename);
			
			char* img_name_buffer_save = malloc(65536); //TODO please make this dynamic maybe
			image_name_buffer_length=0;
			
			for (int i = 0; i < n_images; i++){
				
				char* img_name_end = tmp_img_name_buffer + strlen (tmp_img_name_buffer);
				char* token1 = strtok(tmp_img_name_buffer, "/");
				char* token2 = strtok(filename, "/");
  				char* last_token1;
			    char* last_token2;	
				
				
				while ( token1 && token2 ) {
					last_token1 = token1;
					last_token2 = token2;
					
					tmp_img_name_buffer += strlen(token1) + 1;
					
					if (token2 > filename && token2 < filename_end) token2[-1] = '/';
					
					token1 = strtok(token1 + strlen( token1 ) + 1, "/");
					token2 = strtok(token2 + strlen( token2 ) + 1, "/");
					if (token1 && token2 && strcmp(token1,token2)){
						token2 = strtok(0,"/");
						while(token2){
							token2[-1]='/';
							token2=strtok(0,"/");
							sprintf(img_name_buffer_save + image_name_buffer_length, "../");
							image_name_buffer_length+=3;
						}
						
						if (token1 + strlen(token1) < img_name_end ){
							token1[strlen(token1)] = '/';
						}
						sprintf(img_name_buffer_save + image_name_buffer_length, "%s", token1);
						image_name_buffer_length += strlen(token1) + 1;
						//fwrite(token1,1,strlen(token1)+1,fp );
						tmp_img_name_buffer += strlen(token1) + 1;
						filename[strlen(filename)] = '/';
						break;
					}
					
				}
				if (i < n_images -1){	
					tmp_img_name_buffer = token1 + strlen(token1) + 1;
				}
			}
			fwrite(&image_name_buffer_length,2,1,fp );
			
			fwrite(img_name_buffer_save,1,image_name_buffer_length,fp );
			
			fclose(fp);	
			
			g_free ((gpointer)filename);
  	}
	gtk_widget_destroy (dialog);
		
}

static void import_gif( GtkWidget *bt, gpointer ud ) {
	(void)(bt);
	(void)(ud);
	GtkWidget *dialog;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
	gint res;

	dialog = gtk_file_chooser_dialog_new (
											"Open File",
    	                                  	GTK_WINDOW(win),
        	                              	action,
            	                          	"Cancel",
                	                      	GTK_RESPONSE_CANCEL,
                    	                  	"Open",
                        	              	GTK_RESPONSE_ACCEPT,
                            	          	NULL
										 );

	GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
    
	gtk_file_chooser_set_current_folder (chooser, path_to_res);
	
	GtkFileFilter * filter =gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter,"*.gif");
	gtk_file_filter_set_name (filter,".gif files ");
	gtk_file_chooser_add_filter (chooser, filter);
	
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res != GTK_RESPONSE_ACCEPT)  	{
    	return;
	} 

	const char*  filename = gtk_file_chooser_get_filename (chooser);
	gd_GIF *gif = gd_open_gif(filename);	
   	memcpy( name_buf2 ,	filename, strlen(filename) );
	
	
	char* name = name_buf2;
	
	for (char* name2; (name2 = strstr(name,"/")); name = name2+1);
	
	strstr(name,".")[0] = 0;
	
	uint8_t name_length = strlen(name) + 1;
	memcpy(image_name_buffer+image_name_buffer_length, name,name_length);
	
	image_name_buffer_length += name_length;
	
	for (char* name2; (name2 = strstr(name,"/")); name = name2+1);
			
	char* numbered_name_buffer = 0;	
	int i = 0;
	while (gd_get_frame(gif)) {
		numbered_name_buffer= malloc(32);
		uint8_t* pixelarray = malloc(gif->width * gif->height*3);
		
		gd_render_frame(gif,pixelarray);
        sprintf( numbered_name_buffer, "%s%i", name, i );
		
		VarMapPair vmp={numbered_name_buffer, n_images};
		var_map[var_map_size++]=vmp;
		
			
		GtkWidget* label= gtk_label_new(numbered_name_buffer);
		
		gtk_label_set_xalign (GTK_LABEL(label), 0);
		gtk_container_add(GTK_CONTAINER(imglist_widget), label);
		
				
		memcpy(pixelarray, gif->frame, gif->width * gif->height);

		images[n_images] = (px_image) {
			.width  = gif->width ,
			.height = gif->height,
			.pixels = pixelarray
		};
	 	
		for (int i=0;i<gif->palette->size;i++){
			images[n_images].palette[i].r=gif->palette->colors[i*3+0];
			images[n_images].palette[i].g=gif->palette->colors[i*3+1];
			images[n_images].palette[i].b=gif->palette->colors[i*3+2];
		}
		n_images++;
		i++;

	}
	
	gd_close_gif(gif);
	
	if(!colortable){
		colortable=((uint32_t*)images[0].palette);
		
		glUniform1uiv( glGetUniformLocation(shader,"palette") , 256 ,//  images[0]->format->palette->ncolors,
		(uint32_t*) images[0].palette
		);
		
		gif_palette=malloc(256 *3 );  // TODO maybe rather stack allocate
		
		for (int i=0;i<256;i++){
			gif_palette[i*3]  =images[0].palette[i].r;
			gif_palette[i*3+1]=images[0].palette[i].g;
			gif_palette[i*3+2]=images[0].palette[i].b;
			
			GdkRGBA color= {
					images[0].palette[i].r / 255.f,
					images[0].palette[i].g / 255.f,
					images[0].palette[i].b / 255.f,
									255	   / 255.f
			};
			gtk_widget_override_background_color ( colorbuttons[i], 0, &color);
		}
	}
	

	char strbuf[128];

	sprintf(strbuf,
".l0\n\
set img %s0\n\
.l1\n\
drw 1\n\
cmp $img %s\n\
add img 1\n\
jlt .l1\n\
jmp .l0\n"
		,name
		,numbered_name_buffer		
	);

		
	GtkTextBuffer *txtbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textViews[0]));
	gtk_text_buffer_set_text (txtbuffer,
    strbuf,
    strlen(strbuf));
	//free (strbuf);
	
	
	g_free ((gpointer)filename);
	gtk_widget_destroy (dialog);
		
	gtk_widget_show_all(win);
}

static void destroy_the_gl(GtkWidget *wid, gpointer ud) {
    (void)(wid);
	(void)(ud);
	do_the_gl = FALSE;
}

static gboolean export_gif(){
	gint res;
	GtkWidget *label_scale, *label_frames, 
			  *content_area, *details_dialog, 
			  *spinner_scale, *spinner_f,  
			  *grid;


	details_dialog = gtk_dialog_new_with_buttons("export gif",0 ,0 , 
				  ("_OK"),
                  GTK_RESPONSE_ACCEPT,
                  ("_Cancel"),
                  GTK_RESPONSE_REJECT,
				  NULL
				  );
		
	label_scale  = gtk_label_new("scale:  			");
	label_frames = gtk_label_new("number of frames: ");
		
	spinner_scale 	= gtk_spin_button_new_with_range(1,0xFF,1);
	spinner_f 		= gtk_spin_button_new_with_range(1,0xFFFF,1);
	grid      		= gtk_grid_new();
		
	content_area = gtk_dialog_get_content_area (GTK_DIALOG(details_dialog));
		
	gtk_grid_attach(GTK_GRID(grid), label_scale  ,   0,  0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), label_frames ,   0,  1, 1, 1);
	
	gtk_grid_attach(GTK_GRID(grid), spinner_scale,   1,  0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), spinner_f,   	 1,  1, 1, 1);
	
	gtk_container_add(GTK_CONTAINER(content_area),grid);
	
	gtk_widget_show_all(details_dialog);
	
	res = gtk_dialog_run (GTK_DIALOG (details_dialog));
	
	if (res != GTK_RESPONSE_ACCEPT) return FALSE;


	uint8_t scale = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_scale));
	width_gif    = scale * width; 
	height_gif   = scale * height; 
	frame_count  = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(spinner_f));


	GtkWidget *dialog;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;

	dialog = gtk_file_chooser_dialog_new ("Save File",
                                      GTK_WINDOW(win),
                                      action,
                                      "Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "Save",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);

   	GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
	
	gtk_file_chooser_set_current_folder (chooser, path_to_res);
	
	GtkFileFilter * filter =gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter,"*.gif");
	gtk_file_filter_set_name (filter,".gif files ");
	gtk_file_chooser_add_filter (chooser, filter);
	
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_ACCEPT){
    	char *filename;
	
		gtk_file_chooser_set_current_folder (chooser,  path_to_res);
    	filename = gtk_file_chooser_get_filename (chooser);
    
		rendergif=1;
		framenr=0;
		gif=ge_new_gif(
        	filename,        
        	width_gif, height_gif,   
        	gif_palette, 8,  // TODO set depth by number of colors
         	0              
    	);
	
		g_free ((gpointer)filename);
  	}
	gtk_widget_destroy (dialog);
	gtk_widget_destroy (details_dialog);
	return TRUE;
}

static gboolean on_clicked(GtkWidget *wid, GdkEvent *ev, gpointer user_data) {
    (void)(user_data);
	(void)(wid);
	(void)(ev);
	printf("clicked at %.3fx%.3f with button %d\n",
        ev->button.x, ev->button.y, ev->button.button);
    
	export_gif();
	
	return TRUE;
}

gboolean draw_the_gl(gpointer ud) {
	(void)(ud);
	if (state_flags & paused  ){
		return TRUE; 
	}
	if (framerate_changed){
		g_timeout_add_full(1000, 1000.f / framerate	, draw_the_gl, 0, 0);
    	framerate_changed = 0;
		return FALSE;
	}
		
    GtkWidget *gl = g_gl_wid;
    te_gtkgl_make_current(TE_GTKGL(gl));
	
	glUseProgram(shader);
	if (!do_the_gl)
        return FALSE;


    for (uint16_t i=0; i < 1 +  (frame_count-1) * rendergif; i++){
		
		if(rendergif){
			glViewport(0, 0, width_gif, height_gif);
		}else{
			glViewport(
					gtk_widget_get_allocated_width (gl) / 2 - width  * scale / 2, 
					gtk_widget_get_allocated_height(gl) / 2 - height * scale / 2, 
					width  * scale,
				   	height * scale
			);
	
		}
	
    	glClearColor(0, 0, 0, 1.0);
    	glClear(GL_COLOR_BUFFER_BIT);
   	
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
	
	// Logic	
		uint8_t wait=1;	
		for (uint8_t i=0;i<n_layers;i++){
			
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB ,layers[i].texture);
			while( !(layers[i].wait)  && (layers[i].instr_p < layers[i].n_instr) ){
				
				GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textViews[i]));
				GtkTextIter start_iter;
				GtkTextIter end_iter;
				
				gtk_text_buffer_get_iter_at_line(buffer,&start_iter,lines_of_ips[prev_ips[i]][i] );
				gtk_text_buffer_get_iter_at_line(buffer,&end_iter,lines_of_ips[prev_ips[i]][i] +1);
				
				gtk_text_buffer_remove_tag(buffer,ip_tags[i],&start_iter,&end_iter);
				
				prev_ips[i]=layers[i].instr_p;
				
				instruction instr=layers[i].instr[prev_ips[i]];
				/*printf("l%i %3i %s	%s%i	%s%i	",
						i,
						layers[i].instr_p,
						
						instr.func == add ? "add" :
							instr.func == cmp ? "cmp" :
							instr.func == drw ? "drw" :
							instr.func == jeq ? "jeq" :
							instr.func == jgt ? "jgt" :
							instr.func == jlt ? "jlt" :
							instr.func == jge ? "jge" :
							instr.func == jle ? "jle" :
							instr.func == jmp ? "jmp" :
							instr.func == jne ? "jne" :
												"set" ,

						instr.a1_type ? "$" : " ",
						instr.arg1,
						instr.a2_type ? "$" : " ",
						instr.arg2
						);
				if (instr.a1_type){
					printf("($%i = %i) ", instr.arg1, registers[(uint16_t) instr.arg1]);
				}
				if (instr.a2_type){
					printf("($%i = %i)", instr.arg2, registers[(uint16_t) instr.arg2]);
				}
				printf("\n");
				*/
				gtk_text_buffer_get_iter_at_line(buffer,&start_iter,lines_of_ips[prev_ips[i]][i] );
				gtk_text_buffer_get_iter_at_line(buffer,&end_iter,lines_of_ips[prev_ips[i]][i] +1);
				
				//g_object_set(GTK_TEXT_TAG(tag1), "background-set",TRUE, "background-rgba",&color, NULL);
				
				gtk_text_buffer_apply_tag (	buffer,	ip_tags[i], &start_iter, &end_iter	);
				instr.func(
							layers + i,
							instr.a1_type ? registers[(uint16_t) instr.arg1] :instr.arg1,
							instr.a2_type ? registers[(uint16_t) instr.arg2] :instr.arg2
						  );
				
				for(uint8_t ii =0;ii<7;ii++){
					gtk_spin_button_set_value(GTK_SPIN_BUTTON(	show_regs_widgets[ii][i]), registers[0xffff- 8*ii -i ]  );
				}
				
				layers[i].instr_p++;
				if (state_flags & (debug | step))	break;
		    }
			wait = wait && layers[i].wait;
		}
		for (int i=0; wait && i < 8; i++){
			layers[i].wait--;
		}
		if (state_flags & step) {
			state_flags |= paused;
			state_flags &= ~step;
			
		}	
		if (!wait && n_layers) {
			return TRUE;
		}
	// Render
		for (uint8_t i=0;i<n_layers;i++){
			
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB ,layers[i].texture);
		 	px_image* image = images + registers[0xffff-i];
				
			glTexImage2D(
		          GL_TEXTURE_RECTANGLE_ARB ,0,GL_RED,
				  image->width, image->height,
				  0,GL_RED,GL_UNSIGNED_BYTE,image->pixels
			);
	        
			
			
			//printf("%p, %i, %i\n",image,image->width,image->height);		
			
			int16_t x=registers[0xffff - 8*1 - i];
			int16_t y=registers[0xffff - 8*2 - i];
			int16_t w=registers[0xffff - 8*3 - i];
			int16_t h=registers[0xffff - 8*4 - i];
			
			if ( !w ){
				w = image->width;
			}	
			if ( !h ){
				h = image->height;
			}
			
			int16_t shiftx=(registers[0xffff - 8*5 -i] + w)  % w;
			int16_t shifty=(registers[0xffff - 8*6 -i] + h)  % h;
			
			glUniform2ui( glGetUniformLocation(shader,"shift"), shiftx, shifty );
			glUniform2ui( glGetUniformLocation(shader,"size") , w, h );
			
			
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
			
		}
	
		if (gif && rendergif){
	
			uint8_t pixels[width_gif * height_gif];
			glReadPixels(0, 0, width_gif, height_gif, GL_RED, GL_UNSIGNED_BYTE, pixels);
	
			if(framenr < frame_count +1){
				
				for (int row=0; row < height_gif; row++){
					memcpy(
							gif->frame+row*width_gif, 
							pixels+width_gif*height_gif - (row+1)*width_gif,
						   	width_gif
					);
				}
			}
		
			framenr++;
		
			if (framenr == frame_count +1){
				ge_close_gif(gif);
				gif=0;
				rendergif=0;
			}
		
			if(framenr < frame_count +1){
				ge_add_frame(gif, 100.f/framerate);
			}
		
		}
	}
	
	glUseProgram(0);

	glDisable(GL_TEXTURE_RECTANGLE_ARB);	
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
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
	(void)(ud);
	(void)(bt);
	n_layers=0;
	
	framenr=0;
	memset(registers, 0, 0x010000*2);	

	for (uint8_t i =0;i<8;i++){
		layers[i].wait=0;	
		VarMapPair vmp1 = {
			"img",
			0xffff-8*0-i
		};
		VarMapPair vmp2 = {
			"posx",
			0xffff-8*1-i
		};
		VarMapPair vmp3 = {
			"posy",
			0xffff-8*2-i
		};
		VarMapPair vmp4 = {
			"width",
			0xffff-8*3-i
		};
		VarMapPair vmp5 = {
			"height",
			0xffff-8*4-i
		};
		VarMapPair vmp6 = {
			"shiftx",
			0xffff-8*5-i
		};
		VarMapPair vmp7 = {
			"shifty",
			0xffff-8*6-i
		};


		layers[i].var_map[0]=vmp1;
		layers[i].var_map[1]=vmp2;
		layers[i].var_map[2]=vmp3;
		layers[i].var_map[3]=vmp4;
		layers[i].var_map[4]=vmp5;
		layers[i].var_map[5]=vmp6;
		layers[i].var_map[6]=vmp7;
		layers[i].var_map_size=7;

		layers[i].n_instr=0;
		layers[i].instr_p=0;
			
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
		
		uint16_t linenr=-1;

		while (line){
			
			char* f=strtok(line, " ");
			char* a1=strtok(0, " ");
			char* a2=strtok(0, " ");
			void (*func)(layer*,short arg1,short arg2);
			
			line=nextline;
			linenr++;

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
			
				
			
			
			int16_t arg1=0;
			int16_t arg2=0;
			
			uint8_t type1=0;
			uint8_t type2=0;

			if ( a1[0]=='$' ){
				a1++;
				type1=1;
			}
			
			if ( isdigit(a1[0]) || a1[0]=='-'  ){
				arg1=atoi(a1);
			}
			else if (a1[0]=='.') {
				char found=0;
				for (int l=0;l<n_labels;l++){
					if ( !strcmp( a1+1,labels[l]) ){
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
				if ( varmap_contains(layers[i].var_map,layers[i].var_map_size , a1)  ){
					arg1=getVar(layers[i].var_map,layers[i].var_map_size,a1);
				} else {
					arg1=getVar(var_map,var_map_size,a1);
				}
			}
			
			
			
			if (!a2){
				arg2=0;
			}
			else {
				if ( a2[0]=='$' ){
					a2++;
					type2=1;
				}
				
				if ( isdigit(*a2) || a2[0]=='-' ){
					arg2=atoi(a2);
				}
				else {
					if ( varmap_contains(layers[i].var_map,layers[i].var_map_size , a2)  ){
						arg2=getVar(layers[i].var_map,layers[i].var_map_size,a2);
					} else {
						arg2=getVar(var_map,var_map_size,a2);
					}
				}
			
			}	
			
			instruction in={
				.func=func,
				.arg1= arg1,
				.arg2= arg2,
				.a1_type=type1,
				.a2_type=type2
			};
			
			
			
			layers[i].instr[layers[i].n_instr] = in;
			lines_of_ips[layers[i].n_instr][i] = linenr;
			layers[i].n_instr++;
			
				
			f=strtok(0, " ");
			
				
		}

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
	
}

static unsigned int compileShader(unsigned int type,const char* src){
	unsigned int id= glCreateShader(type);
	
	glShaderSource(id,1, &src,0);
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
	(void)(bt);
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
	char* vs;
	char* fs;
	
	{
	strcpy(path_from_res,"basic.vs" );
	FILE *fp1=fopen(path_to_res,"r");
	fseek(fp1, 0L, SEEK_END);
	uint32_t sz = ftell(fp1);
	fseek(fp1, 0L, SEEK_SET);
	vs = (char*)calloc(sz,sizeof(char));
	fread( vs, sizeof(char), sz,fp1 );
	fclose(fp1);
	}
	{
	strcpy(path_from_res,"basic.fs" );
	FILE *fp2=fopen(path_to_res,"r");
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
	*((uint16_t*)ud) = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner));
}

void framerate_change_func(GtkWidget *spinner, gpointer ud){
	(void)(ud);
	framerate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner));
	framerate_changed = 1;
}

static gboolean select_transparent_color(GtkWidget* w, GdkEvent* e, gpointer user_data) {
	(void)(w);
	(void)(e);
	printf("transparent color index=  %i  \n",(uint8_t) (uint64_t) user_data ); transparent_color_index =(uint8_t) (uint64_t) user_data;
	glUniform1ui( glGetUniformLocation(shader,"transparent_index") ,(uint8_t) (uint64_t) user_data);
	return 1;
}

int main(int argc, char *argv[]) {
	strcpy(path_to_res,argv[0]);
	strcpy(path_to_res+strlen(argv[0]) -4, "../res/");
	path_from_res = path_to_res + strlen(path_to_res);
	
    GtkWidget *palette_grid, 
			  *cnt, *grid_down, *grid_buttons,
			  *gl, 
			  *bt1,*bt2,*bt3,*bt4,*bt5,*bt6,
			  *label_width, *label_height, *label_framerate,
			  *spinner_x, *spinner_y, *spinner_framerate,
			  *gif_import_button,
			  *button_debug, *button_step
				;
    
    gtk_init(&argc, &argv);
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(gtk_main_quit), 0);

    g_gl_wid = gl = te_gtkgl_new();


    cnt = gtk_grid_new();
	palette_grid = gtk_grid_new();
	grid_down = gtk_grid_new();
	grid_buttons = gtk_grid_new();
		

    bt1 = gtk_button_new_with_label("(>)");
	bt2 = gtk_button_new_with_label("||");
	
	button_debug = gtk_button_new_with_label("debug");
	button_step = gtk_button_new_with_label("step");

	label_width  = gtk_label_new("width:");
	label_height = gtk_label_new("height:");
    label_framerate = gtk_label_new("fps:");

	spinner_x = gtk_spin_button_new_with_range(0,65535,1);
	spinner_y = gtk_spin_button_new_with_range(0,65535,1);
	spinner_framerate = gtk_spin_button_new_with_range(0,255,1);


	
	g_signal_connect(G_OBJECT(spinner_x), "value_changed", G_CALLBACK(spinner_value_changed), (gpointer)&width);
	g_signal_connect(G_OBJECT(spinner_y), "value_changed", G_CALLBACK(spinner_value_changed), (gpointer)&height);
	
	g_signal_connect(G_OBJECT(spinner_framerate), "value_changed", G_CALLBACK(framerate_change_func), (gpointer)&height);
	
	g_signal_connect(G_OBJECT(bt1), "clicked", 			G_CALLBACK(refresh), 0	);
	g_signal_connect(G_OBJECT(bt2), "clicked", 			G_CALLBACK(toggle_flag_func), (gpointer) paused  );
	g_signal_connect(G_OBJECT(button_debug), "clicked", G_CALLBACK(toggle_flag_func), (gpointer) debug  );
	g_signal_connect(G_OBJECT(button_step),  "clicked", G_CALLBACK(step_func), (gpointer) step  );
	
    
	// set a callback that will stop the timer from drawing
    g_signal_connect(G_OBJECT(gl), "destroy", G_CALLBACK(destroy_the_gl), 0);

    gtk_widget_add_events(gl,  GDK_ALL_EVENTS_MASK);
    g_signal_connect(G_OBJECT(gl), "button-press-event", G_CALLBACK(on_clicked), 0);
    
	gtk_widget_add_events(win, GDK_KEY_PRESS_MASK);

	g_signal_connect(G_OBJECT(gl), "scroll-event",    G_CALLBACK(zoom), (gpointer) 1 );
    g_signal_connect(G_OBJECT(win), "key_press_event", G_CALLBACK(zoom), (gpointer) 0 );

	gtk_widget_set_size_request(palette_grid, 140, 140);
    
	for (uint64_t i = 0; i< 256; i++ ){
		
		colorbuttons[i]=gtk_event_box_new();
    	gtk_widget_add_events(colorbuttons[i],  GDK_BUTTON_PRESS_MASK);

			
	    g_signal_connect(
						G_OBJECT(colorbuttons[i]), 
						"button-press-event", 
						G_CALLBACK(select_transparent_color),
						(gpointer) i
						);
			
		gtk_widget_set_size_request(colorbuttons[i], 16, 16);
		gtk_grid_attach(GTK_GRID(palette_grid), colorbuttons[i],   i%16,  i/16, 1, 1);
		
	}
	
	GdkRGBA color= {65535, 0, 0, 0xffff};	
	gtk_widget_override_background_color ( colorbuttons[0], 0, &color);

	GdkRGBA color2={0, 65535, 0, 0xffff};	
	gtk_widget_override_background_color ( colorbuttons[1], 0, &color2);

	GdkRGBA color3={0, 0, 65535, 0xffff};	
	gtk_widget_override_background_color ( colorbuttons[2], 0, &color3);
	
	gtk_grid_attach(GTK_GRID(cnt), gl,    0,  0, 1, 2);
    gtk_grid_attach(GTK_GRID(cnt), palette_grid,   2,  0, 1, 1);
	
  	gtk_grid_attach(GTK_GRID(grid_buttons), bt1,   0,  0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_buttons), bt2,   1,  0, 1, 1);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_x),         width );
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_y),         height);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_framerate), framerate);
  	
	gtk_grid_attach(GTK_GRID(grid_buttons), label_width,    	0,  1, 1, 1);
  	gtk_grid_attach(GTK_GRID(grid_buttons), spinner_x,      	1,  1, 1, 1);
  	gtk_grid_attach(GTK_GRID(grid_buttons), label_height,   	0,  2, 1, 1);
  	gtk_grid_attach(GTK_GRID(grid_buttons), spinner_y,      	1,  2, 1, 1);
    
  	gtk_grid_attach(GTK_GRID(grid_buttons), label_framerate,    0,  3, 1, 1);
  	gtk_grid_attach(GTK_GRID(grid_buttons), spinner_framerate,  1,  3, 1, 1);

  	gtk_grid_attach(GTK_GRID(grid_buttons), button_debug ,   0,  4, 1, 1);
	gtk_grid_attach(GTK_GRID(grid_buttons), button_step  ,   1,  4, 1, 1);

	gtk_grid_attach(GTK_GRID(cnt), grid_buttons,   2,  1, 1, 1);
	GtkWidget* frames[8];
	GtkWidget* scrollpanes[9];
	GtkWidget* varlist[8];

	for (uint8_t i = 0;i<8;i++){
		char namebuffer[8]="Layer__";
		namebuffer[6]=48+i;
		frames[i] =gtk_frame_new(namebuffer);
		scrollpanes[i] = gtk_scrolled_window_new(NULL, NULL);
		textViews[i] = gtk_text_view_new();
		
		gtk_container_add(GTK_CONTAINER(frames[i]),scrollpanes[i]);
		gtk_container_add(GTK_CONTAINER(scrollpanes[i]),textViews[i]);
		
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textViews[i]));

	 	ip_tags[i]= gtk_text_buffer_create_tag( buffer,	0, "background", "#000040",	NULL );

		gtk_widget_set_size_request(scrollpanes[i], 0, 220);
		gtk_widget_set_hexpand(scrollpanes[i],1);
		gtk_widget_set_vexpand(scrollpanes[i],1);
		
		
		gtk_grid_attach(GTK_GRID(grid_down), frames[i], i,  0, 1, 1);
		
		varlist[i] = gtk_grid_new ();
		{
			GtkWidget* label= gtk_label_new(" image:");
			show_regs_widgets[0][i] = gtk_spin_button_new_with_range(-0x8000,0x7fff,1);
			g_signal_connect(G_OBJECT(show_regs_widgets[0][i]), "value_changed", G_CALLBACK(spinner_value_changed), (gpointer)(registers+0xffff-8*0-i));
			gtk_label_set_xalign (GTK_LABEL(label), 0);
			gtk_grid_attach(GTK_GRID(varlist[i]), label  , 0, 0, 1, 1);
			gtk_grid_attach(GTK_GRID(varlist[i]), show_regs_widgets[0][i], 1, 0, 1, 1);
		}
		{
			GtkWidget* label= gtk_label_new(" posx:");
			show_regs_widgets[1][i] = gtk_spin_button_new_with_range(-0x8000,0x7fff,1);
			g_signal_connect(G_OBJECT(show_regs_widgets[1][i]), "value_changed", G_CALLBACK(spinner_value_changed), (gpointer)(registers+0xffff-8*1-i));
			gtk_label_set_xalign (GTK_LABEL(label), 0);
			gtk_grid_attach(GTK_GRID(varlist[i]), label  , 0, 1, 1, 1);
			gtk_grid_attach(GTK_GRID(varlist[i]), show_regs_widgets[1][i], 1, 1, 1, 1);
		}
		{
			GtkWidget* label= gtk_label_new(" posy:");
			show_regs_widgets[2][i] = gtk_spin_button_new_with_range(-0x8000,0x7fff,1);
			g_signal_connect(G_OBJECT(show_regs_widgets[2][i]), "value_changed", G_CALLBACK(spinner_value_changed), (gpointer)(registers+0xffff-8*2-i));
			gtk_label_set_xalign (GTK_LABEL(label), 0);
			gtk_grid_attach(GTK_GRID(varlist[i]), label  , 0, 2, 1, 1);
			gtk_grid_attach(GTK_GRID(varlist[i]), show_regs_widgets[2][i], 1, 2, 1, 1);
		}
		{
			GtkWidget* label= gtk_label_new(" width:");
			show_regs_widgets[3][i] = gtk_spin_button_new_with_range(-0x8000,0x7fff,1);
			g_signal_connect(G_OBJECT(show_regs_widgets[3][i]), "value_changed", G_CALLBACK(spinner_value_changed), (gpointer)(registers+0xffff-8*3-i));
			gtk_label_set_xalign (GTK_LABEL(label), 0);
			gtk_grid_attach(GTK_GRID(varlist[i]), label  , 0, 3, 1, 1);
			gtk_grid_attach(GTK_GRID(varlist[i]), show_regs_widgets[3][i], 1, 3, 1, 1);
		}
		{
			GtkWidget* label= gtk_label_new(" height:");
			show_regs_widgets[4][i] = gtk_spin_button_new_with_range(-0x8000,0x7fff,1);
			g_signal_connect(G_OBJECT(show_regs_widgets[4][i]), "value_changed", G_CALLBACK(spinner_value_changed), (gpointer)(registers+0xffff-8*4-i));
			gtk_label_set_xalign (GTK_LABEL(label), 0);
			gtk_grid_attach(GTK_GRID(varlist[i]), label  , 0, 4, 1, 1);
			gtk_grid_attach(GTK_GRID(varlist[i]), show_regs_widgets[4][i], 1, 4, 1, 1);
		}
		{
			GtkWidget* label= gtk_label_new(" shiftx:");
			show_regs_widgets[5][i] = gtk_spin_button_new_with_range(-0x8000,0x7fff,1);
			g_signal_connect(G_OBJECT(show_regs_widgets[5][i]), "value_changed", G_CALLBACK(spinner_value_changed), (gpointer)(registers+0xffff-8*5-i));
			gtk_label_set_xalign (GTK_LABEL(label), 0);
			gtk_grid_attach(GTK_GRID(varlist[i]), label  , 0, 5, 1, 1);
			gtk_grid_attach(GTK_GRID(varlist[i]), show_regs_widgets[5][i], 1, 5, 1, 1);
		}
		{
			GtkWidget* label= gtk_label_new(" shifty:");
			show_regs_widgets[6][i] = gtk_spin_button_new_with_range(-0x8000,0x7fff,1);
			
			g_signal_connect(G_OBJECT(show_regs_widgets[6][i]), "value_changed", 
							G_CALLBACK(spinner_value_changed), (gpointer)(registers+0xffff-8*6-i));
			
			gtk_label_set_xalign (GTK_LABEL(label), 0);
			gtk_grid_attach(GTK_GRID(varlist[i]), label  , 0, 6, 1, 1);
			gtk_grid_attach(GTK_GRID(varlist[i]), show_regs_widgets[6][i], 1, 6, 1, 1);
		}
		gtk_grid_attach(GTK_GRID(grid_down), varlist[i], i,  1, 1, 1);
			
	}
	
	gtk_grid_attach(GTK_GRID(cnt),grid_down,0,2,3,1);

	
	scrollpanes[8] = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scrollpanes[8], 140, 400);
		
	imglist_widget = gtk_list_box_new ();
	
	gtk_container_add(GTK_CONTAINER(scrollpanes[8]), imglist_widget);
    
	gtk_grid_attach(GTK_GRID(cnt), scrollpanes[8],   1,  0, 1, 2);
 	
	menubar = gtk_menu_bar_new();

	GtkWidget* menuitem0 = gtk_menu_item_new_with_label("file") ;
	GtkWidget* vbox 	 = gtk_vbox_new(0,2);
	
	GtkWidget* submenu   = gtk_menu_new();

	bt3 = gtk_menu_item_new_with_label("load");
	bt4 = gtk_menu_item_new_with_label("save");
	bt5 = gtk_menu_item_new_with_label("import images");
	bt6 = gtk_menu_item_new_with_label("export as gif");
	gif_import_button = gtk_menu_item_new_with_label("import gif");


	gtk_menu_shell_append(GTK_MENU(submenu), bt3 );
	gtk_menu_shell_append(GTK_MENU(submenu), bt4 );
	gtk_menu_shell_append(GTK_MENU(submenu), bt5 );
	gtk_menu_shell_append(GTK_MENU(submenu), bt6 );
	gtk_menu_shell_append(GTK_MENU(submenu), gif_import_button );

	g_signal_connect(G_OBJECT(bt3), "activate", G_CALLBACK(load_func), 0);
	g_signal_connect(G_OBJECT(bt4), "activate", G_CALLBACK(save_func), 0);
	g_signal_connect(G_OBJECT(bt5), "activate", G_CALLBACK(load_images_func), 0);
	g_signal_connect(G_OBJECT(bt6), "activate", G_CALLBACK(export_gif), 0);
	g_signal_connect(G_OBJECT(gif_import_button), "activate", G_CALLBACK(import_gif), 0);
	
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem0), submenu );


	gtk_container_add(GTK_CONTAINER(menubar), menuitem0 );
	
	gtk_container_add(GTK_CONTAINER(vbox),menubar );
    gtk_container_add(GTK_CONTAINER(vbox), cnt );
	
    gtk_container_add(GTK_CONTAINER(win), vbox );
	
	gtk_widget_set_size_request(gl, 1024, 512);
    
	strcpy(path_from_res,"icon.bmp" );
	gtk_window_set_icon_from_file(GTK_WINDOW(win), path_to_res, 0);	
								//this won't work unless called with the right current path
	
	*path_from_res = 0;
	
	gtk_widget_show_all(win);	
	
	
	
    te_gtkgl_make_current(TE_GTKGL(gl));
	if (glewInit() ){
		printf("glew failed\n");
		return 1;
	}	
	shader=createShader();
	glUseProgram(shader);
	
	g_timeout_add_full(1000, 10, draw_the_gl, 0, 0);
	glEnable(GL_TEXTURE_RECTANGLE_ARB);

	
	gtk_main();
    return 0;

}

