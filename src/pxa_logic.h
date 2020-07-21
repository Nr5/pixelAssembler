short registers[64];

typedef struct px_color{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
} px_color;

typedef struct px_image{
	uint16_t width;
	uint16_t height;
	px_color palette[256];
	uint8_t* pixels;	
} px_image;

px_image images[256];

uint8_t n_layers = 0;
uint8_t n_images = 0;
int16_t  compval = 0;

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

uint16_t framenr      = 0;
VarMapPair var_map[256];
uint8_t  var_map_size = 0;

struct layer{
	//layerlist* list;
	//layerlist* current;
	instruction instr[1024];
	short registers[64];
	uint16_t n_instr;
	px_image* img;
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
	(void)(arg2);
	this->registers[63]=arg1;
	if ( ((uint16_t)arg1) < n_images){
		this->img=images + arg1;
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB ,0,GL_RED,this->img->width,this->img->height,
						0,GL_RED,GL_UNSIGNED_BYTE,this->img->pixels);
	}
}

void drawinstr (layer* this,short arg1,short arg2){
	(void)(arg2);
	//setimage (this,this->registers[63],0);
	this->wait=arg1;
}

void compare (layer* this,short arg1,short arg2){
	(void)(this);
	compval=arg1-arg2;
}

void jump_not_equal (layer* this,short arg1,short arg2){
	(void)(arg2);
	if (compval && ( ((uint16_t)arg1) < this->n_instr)  ) this->instr_p=arg1-1;
}

void jump_equal (layer* this,short arg1,short arg2){
	(void)(arg2);
	if (!compval && ( ((uint16_t)arg1) < this->n_instr)) this->instr_p=arg1-1;
}

void jump_greater_then (layer* this,short arg1,short arg2){
	(void)(arg2);
	if (compval>0 && ( ((uint16_t)arg1) < this->n_instr)) this->instr_p=arg1-1;
}

void jump_less_then (layer* this,short arg1,short arg2){
	(void)(arg2);
	if (compval<0 && ( ((uint16_t)arg1) < this->n_instr)) this->instr_p=arg1-1;
}

void jump (layer* this,short arg1,short arg2){
	(void)(arg2);
	if ( ((uint16_t)arg1) < this->n_instr) this->instr_p=arg1-1;
}

void move (layer* this,short arg1,short arg2){
	this->posx+=arg1;
	this->posy+=arg2;
	
}

void set (layer* this,short arg1,short arg2){
	(void)(this);
	registers[arg1]=arg2;
}

void add (layer* this,short arg1,short arg2){
	(void)(this);
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

void (*getFunc(const char* name))(layer*, int16_t, int16_t) {
	for (int i=0;i<instr_map_size;i++){
		if (!strcmp(instr_map[i].key ,name)){
			return instr_map[i].val;
		}
	}
	return 0;
}

