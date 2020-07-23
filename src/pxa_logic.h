int16_t registers[0x10000];

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
	instruction instr[1024];
	short registers[65536];
	uint16_t n_instr;
	uint8_t wait;
	uint16_t instr_p;
	unsigned int texture;
	VarMapPair var_map[256];
	uint8_t var_map_size;
	int16_t compval;
};

int16_t getVar(VarMapPair *var_map,uint8_t var_map_size,const char* name) {
	for (int i=0;i<var_map_size;i++){
		if (!strcmp(var_map[i].key ,name)){
			return var_map[i].val;
		}
	}
	return -1;
}
char varmap_contains(VarMapPair *var_map,uint8_t var_map_size,const char* name) {
	for (int i=0;i<var_map_size;i++){
		if (!strcmp(var_map[i].key ,name)){
			return 1;
		}
	}
	return 0;

}

void drw (layer* this,short arg1,short arg2){
	(void)(arg2);
	this->wait=arg1;
}

void cmp (layer* this,short arg1,short arg2){
	this->compval=arg1-arg2;
}

void jne (layer* this,short arg1,short arg2){
	(void)(arg2);
	if (this->compval && ( ((uint16_t)arg1) < this->n_instr)  ) this->instr_p=arg1-1;
}

void jeq (layer* this,short arg1,short arg2){
	(void)(arg2);
	if (!this->compval && ( ((uint16_t)arg1) < this->n_instr)) this->instr_p=arg1-1;
}

void jgt (layer* this,short arg1,short arg2){
	(void)(arg2);
	if (this->compval>0 && ( ((uint16_t)arg1) < this->n_instr)) this->instr_p=arg1-1;
}

void jlt (layer* this,short arg1,short arg2){
	(void)(arg2);
	if (this->compval<0 && ( ((uint16_t)arg1) < this->n_instr)) this->instr_p=arg1-1;
}

void jle (layer* this,short arg1,short arg2){
	(void)(arg2);
	if (this->compval<=0 && ( ((uint16_t)arg1) < this->n_instr)) this->instr_p=arg1-1;
}

void jge (layer* this,short arg1,short arg2){
	(void)(arg2);
	if (this->compval>=0 && ( ((uint16_t)arg1) < this->n_instr)) this->instr_p=arg1-1;
}

void jmp (layer* this,short arg1,short arg2){
	(void)(arg2);
	if ( ((uint16_t)arg1) < this->n_instr) this->instr_p=arg1-1;
}

void set (layer* this,short arg1,short arg2){
	(void)(this);
	registers[ (uint16_t) arg1 ] =arg2;
}

void add (layer* this,short arg1,short arg2){
	(void)(this);
	registers[ (uint16_t) arg1 ] += arg2;
}

typedef struct MapPair{
	const char* key;
	void (*val)(layer* this,short arg1,short arg2);
}MapPair;

MapPair instr_map[]={
	{"add",add},
	{"cmp",cmp},
	{"drw",drw},
	{"jeq",jeq},
	{"jgt",jgt},
	{"jlt",jlt},
	{"jle",jle},
	{"jge",jge},
	{"jmp",jmp},
	{"jne",jne},
	{"set",set},
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

