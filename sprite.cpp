#include "sprite.h"

HWND hwndSprite;
bool wvisSprite = false;
level_sprite_data_ctx_t spriteContexts[2];
int curSpCtx = 0;

////////////////////////////////
//SPRITE OCCUPY REGISTRATION &//
// 8x8/16x16 SECTION HANDLING //
////////////////////////////////
void addSpriteTile(sprite_t * s,BYTE props,DWORD tile,int offsX,int offsY) {
	//Simple setup
	sprite_tile_t entry;
	entry.props		 = props;
	entry.tile		 = tile;
	entry.offsX		 = offsX;
	entry.offsY		 = offsY;
	//Calculate occupied tile data
	int spX = s->data[2]<<4;
	int spY = (s->data[1]&0xFE)<<3;
	entry.numOccupiedTiles = 0;
	switch(tile&0xC000) {
		case 0x0000: {
			if(props&1) {
				//16x16
				entry.numOccupiedTiles = 1;
				if(offsX&0xF) entry.numOccupiedTiles <<= 1;
				if(offsY&0xF) entry.numOccupiedTiles <<= 1;
				//TODO
			} else {
				//8x8
				entry.numOccupiedTiles = 1;
				if((offsX&0xF)>8) entry.numOccupiedTiles <<= 1;
				if((offsY&0xF)>8) entry.numOccupiedTiles <<= 1;
				//TODO
			}
			break;
		}
		case 0x4000: {
			//16x16
			entry.numOccupiedTiles = 1;
			if(offsX&0xF) entry.numOccupiedTiles <<= 1;
			if(offsY&0xF) entry.numOccupiedTiles <<= 1;
			//TODO
			break;
		}
		case 0x8000: {
			//256x1
			entry.numOccupiedTiles = 16;
			if(offsX&0xF) entry.numOccupiedTiles++;
			//TODO
			break;
		}
		case 0xC000: {
			entry.numOccupiedTiles = 1;
			if((offsX&0xF)>8) entry.numOccupiedTiles <<= 1;
			if((offsY&0xF)>8) entry.numOccupiedTiles <<= 1;
			//TODO
			break;
		}
	}
	//Store in sprite
	s->tiles.push_back(entry);
}

////////////////////////////
//SPRITE DRAWING FUNCTIONS//
////////////////////////////
BYTE spGfxFiles[6];

//Helper function for drawing text
void drawSpriteText(sprite_t * s,const char * text) {
	int offsX = 0;
	int offsY = 0;
	while(true) {
		char c = *(text++);
		if(c=='\0') break;
		else if(c=='\n') {
			offsX = 0;
			offsY += 8;
			continue;
		} else {
			addSpriteTile(s,0,0xC000|c,offsX,offsY);
			offsX += 8;
		}
	}
}
//Helper function for drawing HDMA sprites from polygon data
void drawSpriteHDMAPolygon(sprite_t * s,BYTE * data,int coneBase,int numPoints,bool ux,bool uy,int offsX,int offsY) {
	int lineLeft[0x100],lineRight[0x100];
	for(int j=0; j<0x100; j++) {
		lineLeft[j] = 1000;
		lineRight[j] = -1000;
	}
	for(int n=0; n<numPoints; n++) {
		int curX = data[(n<<1)];
		int curY = data[(n<<1)+1];
		int nextX = data[(n<<1)+2];
		int nextY = data[(n<<1)+3];
		if((n+1)==numPoints) {
			nextX = data[0];
			nextY = data[1];
		}
		if(!ux) {
			if(curX&0x80) curX -= 0x100;
			if(nextX&0x80) nextX -= 0x100;
		}
		if(!uy) {
			curY ^= 0x80;
			nextY ^= 0x80;
		}
		if(curY<nextY) {
			int dx = (nextX-curX)<<16;
			int dy = nextY-curY;
			int dxdy = dx/dy;
			int x = curX<<16;
			for(int j=curY; j<=nextY; j++) {
				int rhs = x>>16;
				if(rhs>lineRight[j]) lineRight[j] = rhs;
				x += dxdy;
			}
		} else if(curY>nextY) {
			int dx = (nextX-curX)<<16;
			int dy = curY-nextY;
			int dxdy = dx/dy;
			int x = curX<<16;
			for(int j=curY; j>=nextY; j--) {
				int lhs = x>>16;
				if(lhs<lineLeft[j]) lineLeft[j] = lhs;
				x += dxdy;
			}
		}
	}
	for(int i=0; i<0x100; i++) {
		if(lineLeft[i]<lineRight[i]) {
			int width = lineRight[i]-lineLeft[i];
			if(width>0xFF) width = 0xFF;
			int offsX2 = (lineRight[i]+lineLeft[i])>>1;
			if(width&1) offsX2++;
			int offsY2 = uy?0:0x80;
			addSpriteTile(s,0,coneBase+width,offsX+offsX2-0x80,offsY+i-offsY2);
		}
	}
}

//Helper function for finding GFX files
inline int findSpGfxFile(BYTE file) {
	for(int i=5; i>=0; i--) {
		if(spGfxFiles[i]==file) return 0x100+(i<<5);
	}
	return 0;
}

//SuperFX texture displayer function
void dispSuperFXTexture(DWORD * pixelBuf,int width,int height,BYTE props,WORD tile,POINT offs) {
	int offsX = offs.x;
	int offsY = offs.y;
	bool flipV = props&0x80;
	bool flipH = props&0x40;
	int palette = (props&0x3C)<<2;
	int tx = (tile&0xF)<<4;
	int ty = tile&0x3F0;
	for(int j=0; j<16; j++) {
		for(int i=0; i<16; i++) {
			int sx = flipH?(0xF-i):i;
			int sy = flipV?(0xF-j):j;
			if(tile&0x2000) {
				int temp = sx;
				sx = sy;
				sy = temp;
			}
			sx += tx;
			sy += ty;
			int dx = offsX+i;
			int dy = offsY+j;
			int pixShift = (tile&0x400)?4:0;
			int idx = getIndexFromTexture(&romBuf[0x140000],{sx,sy});
			idx = (idx>>pixShift)&0xF;
			if(idx) {
				putPixel(pixelBuf,width,height,paletteBuffer[palette|idx],{dx,dy});
			}
		}
	}
}
//HDMA displayer function
void dispBackgroundRow(DWORD * pixelBuf,int width,int height,int row,POINT offs) {
	int offsX = offs.x;
	int offsY = offs.y;
	int base = ((row&0x3FF)<<10)|((row&0x400)>>1);
	for(int i=0; i<0x100; i++) {
		DWORD color = bmpDataBg[base+i];
		if(color) {
			putPixel(pixelBuf,width,height,color,{offsX+i,offsY});
		}
	}
}

//Floating log
void drawSprite_000(sprite_t * s) {
	addSpriteTile(s,(0xC<<2),0x4046,-8,-8);
	addSpriteTile(s,(0xC<<2),0x4047,8,-8);
	addSpriteTile(s,(0xC<<2),0x4056,-8,8);
	addSpriteTile(s,(0xC<<2),0x4057,8,8);
}
//Closed door
void drawSprite_001(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x4502,-8,-16);
	addSpriteTile(s,(0x8<<2)|0x40,0x4502,8,-16);
	addSpriteTile(s,(0x8<<2),0x4512,-8,0);
	addSpriteTile(s,(0x8<<2)|0x40,0x4512,8,0);
}
//Naval Piranha stalk
void drawSprite_002(sprite_t * s) {
	int base = findSpGfxFile(0x5A);
	addSpriteTile(s,(0x9<<2)|0x41,base,-8,-8);
	addSpriteTile(s,(0x9<<2)|1,base,8,-8);
	addSpriteTile(s,(0x9<<2)|0x41,base+0x08,-16,2);
	addSpriteTile(s,(0x9<<2)|1,base+0x08,16,2);
}
//Crate
void drawSprite_003(sprite_t * s) {
	addSpriteTile(s,(0xC<<2),0x4128,-8,-16);
	addSpriteTile(s,(0xC<<2),0x4129,8,-16);
	addSpriteTile(s,(0xC<<2),0x4138,-8,0);
	addSpriteTile(s,(0xC<<2),0x4139,8,0);
}
//Star Mario block item
void drawSprite_004(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|1,0x00C0,0,0);
}
//Icy watermelon
void drawSprite_005(sprite_t * s) {
	addSpriteTile(s,(0xB<<2)|1,0x0068,0,0);
}
//Icy watermelon projectile
void drawSprite_006(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x25D0,0,0);
	addSpriteTile(s,(0x9<<2),0x25D1,8,0);
	addSpriteTile(s,(0x9<<2),0x25D8,0,8);
	addSpriteTile(s,(0x9<<2),0x25D9,8,8);
}
//Watermelon
void drawSprite_007(sprite_t * s) {
	addSpriteTile(s,(0x8<<2)|1,0x0068,0,0);
}
//Rubble from final boss
void drawSprite_008(sprite_t * s) {
	addSpriteTile(s,(0xE<<2),0x408E,-8,-8);
	addSpriteTile(s,(0xE<<2),0x408F,8,-8);
	addSpriteTile(s,(0xE<<2),0x409E,-8,8);
	addSpriteTile(s,(0xE<<2),0x409F,8,8);
}
//Fire watermelon
void drawSprite_009(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|1,0x0068,0,0);
}
//Kaboomba
void drawSprite_00A(sprite_t * s) {
	int base = findSpGfxFile(0x4A);
	addSpriteTile(s,(0xA<<2),base+0x03,0,-8);
	addSpriteTile(s,(0xA<<2),base+0x0F,0,8);
	addSpriteTile(s,(0xA<<2)|1,base,-8,0);
	addSpriteTile(s,(0xA<<2)|1,base+0x01,0,0);
	addSpriteTile(s,(0xA<<2),base+0x0F,4,8);
}
//Kaboomba projectile
void drawSprite_00B(sprite_t * s) {
	int base = findSpGfxFile(0x4A);
	addSpriteTile(s,(0xA<<2)|1,base+0x0C,0,0);
}
//Raphael Raven
void drawSprite_00C(sprite_t * s) {
	for(int j=0; j<4; j++) {
		for(int i=0; i<4; i++) {
			int offsX = -24+(i<<4);
			int offsY = -24+(j<<4);
			addSpriteTile(s,(0xE<<2),0x4508+i+(j<<4),offsX,offsY);
		}
	}
}
//Goal Ring
void drawSprite_00D(sprite_t * s) {
	addSpriteTile(s,(0xA<<2),0x4034,24,-16);
	addSpriteTile(s,(0x9<<2),0x4041,13,-26);
	addSpriteTile(s,(0xA<<2),0x4034,6,-50);
	addSpriteTile(s,(0x9<<2),0x4041,6,-79);
	addSpriteTile(s,(0xA<<2),0x4034,13,-103);
	addSpriteTile(s,(0x9<<2),0x4041,24,-112);
	addSpriteTile(s,(0xA<<2),0x4034,34,-103);
	addSpriteTile(s,(0x9<<2),0x4041,41,-79);
	addSpriteTile(s,(0xA<<2),0x4034,41,-49);
	addSpriteTile(s,(0x9<<2),0x4041,34,-25);
}
//GOAL!
void drawSprite_00E(sprite_t * s) {
	for(int i=0; i<5; i++) {
		int i2 = 4-i;
		int base = romBuf[0x04AD6B+(i2<<1)]|(romBuf[0x04AD6C+(i2<<1)]<<8);
		base += 0x040000;
		int numTiles = romBuf[0x04AD66+i2];
		int baseX = romBuf[0x04AD61+i];
		for(int n=0; n<numTiles; n++) {
			int offsX = romBuf[base+(n*5)];
			int offsY = romBuf[base+(n*5)+1];
			if(offsX&0x80) offsX -= 0x100;
			if(offsY&0x80) offsY -= 0x100;
			int tileRef = romBuf[base+(n*5)+2]|(romBuf[base+(n*5)+3]<<8);
			WORD tile = tileRef&0x01FF;
			BYTE sz = (tileRef>>8)&0xC0;
			if(romBuf[base+(n*5)+4]) sz++;
			addSpriteTile(s,(0xB<<2)|sz,tile,baseX+offsX,offsY);
		}
	}
}
//BONUS CHALLENGE
void drawSprite_00F(sprite_t * s) {
	for(int n=0; n<46; n++) {
		int offsX = romBuf[0x078276+(n*5)];
		int offsY = romBuf[0x078277+(n*5)];
		if(offsX&0x80) offsX -= 0x100;
		if(offsY&0x80) offsY -= 0x100;
		int tileRef = romBuf[0x078278+(n*5)]|(romBuf[0x078279+(n*5)]<<8);
		WORD tile = tileRef&0x01FF;
		BYTE sz = (tileRef>>8)&0xC0;
		if(romBuf[0x07827A+(n*5)]) sz++;
		addSpriteTile(s,(0xE<<2)|sz,tile,offsX,offsY);
	}
}
//Caged Ghost round mound
void drawSprite_010(sprite_t * s) {
	drawSpriteHDMAPolygon(s,&romBuf[0x036407],0x8400,20,false,false,0,0);
	int base = findSpGfxFile(0x42);
	addSpriteTile(s,(0x9<<2)|1,base+0x26,-5,-105);
	addSpriteTile(s,(0x9<<2)|1,base+0x24,-17,-105);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,-16,-88);
	addSpriteTile(s,(0x9<<2)|1,base+0x02,0,-88);
	addSpriteTile(s,(0x9<<2)|1,base+0x22,0,-76);
	addSpriteTile(s,(0x9<<2)|1,base+0x20,-16,-76);
	addSpriteTile(s,(0x9<<2),base+0x3B,0,-99);
	addSpriteTile(s,(0x9<<2),base+0x3B,-10,-99);
}
//Minigame prize card
void drawSprite_011(sprite_t * s) {
	addSpriteTile(s,(0xE<<2),0x4177,-8,-8);
	addSpriteTile(s,(0xE<<2),0x4178,8,-8);
	addSpriteTile(s,(0xE<<2),0x4187,-8,8);
	addSpriteTile(s,(0xE<<2),0x4188,8,8);
}
//Boss door
void drawSprite_012(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x416C,-8,-16);
	addSpriteTile(s,(0x9<<2)|0x40,0x416C,8,-16);
	addSpriteTile(s,(0x9<<2),0x417C,-8,0);
	addSpriteTile(s,(0x9<<2)|0x40,0x417C,8,0);
}
//Boss explosion
void drawSprite_013(sprite_t * s) {
	addSpriteTile(s,(0xB<<2),0x0055,0,0);
	addSpriteTile(s,(0xB<<2),0x0055,8,0);
	addSpriteTile(s,(0xB<<2),0x0055,0,8);
	addSpriteTile(s,(0xB<<2),0x0055,8,8);
}
//Boss key
void drawSprite_014(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x40CE,-8,-8);
	addSpriteTile(s,(0x9<<2),0x40CF,8,-8);
	addSpriteTile(s,(0x9<<2),0x40DE,-8,8);
	addSpriteTile(s,(0x9<<2),0x40DF,8,8);
}
//Submarine Yoshi projectile
void drawSprite_015(sprite_t * s) {
	addSpriteTile(s,(0xD<<2)|0x80,0x603B,0,0);
}
//Bigger Boo
void drawSprite_016(sprite_t * s) {
	for(int i=0; i<120; i++) {
		int offsX = romBuf[0x023D8E +(i<<1)];
		int width = romBuf[0x023D8F+(i<<1)];
		if(offsX&0x80) offsX -= 0x100;
		if(width&1) offsX++;
		addSpriteTile(s,0,0x8000+width,offsX-0x80,i-60);
	}
	addSpriteTile(s,(0x9<<2)|0x40,0x4545,0,-48);
	addSpriteTile(s,(0x9<<2)|0x40,0x4544,16,-48);
	addSpriteTile(s,(0x9<<2)|0x40,0x4555,0,-32);
	addSpriteTile(s,(0x9<<2)|0x40,0x4554,16,-32);
	addSpriteTile(s,(0x9<<2)|0x40,0x4565,0,-16);
	addSpriteTile(s,(0x9<<2)|0x40,0x4564,16,-16);
	addSpriteTile(s,(0x9<<2)|0x40,0x4575,0,0);
	addSpriteTile(s,(0x9<<2)|0x40,0x4574,16,0);
}
//Frog Pirate
void drawSprite_017(sprite_t * s) {
	int base = findSpGfxFile(0x68);
	addSpriteTile(s,(0x9<<2),base+0x08,-2,2);
	addSpriteTile(s,(0x9<<2),base+0x18,-2,10);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,-8,0);
	addSpriteTile(s,(0x9<<2)|1,base+0x01,0,0);
	addSpriteTile(s,(0x9<<2),base+0x0C,-3,-5);
	addSpriteTile(s,(0x9<<2),base+0x0C,4,-5);
	addSpriteTile(s,(0x9<<2),base+0x08,14,2);
	addSpriteTile(s,(0x9<<2),base+0x18,14,10);
	addSpriteTile(s,(0x9<<2),base+0x06,5,10);
	addSpriteTile(s,(0x9<<2),base+0x06,-4,10);
}
//Fire watermelon projectile
void drawSprite_018(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x25F0,0,0);
	addSpriteTile(s,(0x9<<2),0x25F1,8,0);
	addSpriteTile(s,(0x9<<2),0x25F8,0,8);
	addSpriteTile(s,(0x9<<2),0x25F9,8,8);
}
//Bubble
void drawSprite_019(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|0x41,0x00A2,0,0);
}
//Ski lift
void drawSprite_01A(sprite_t * s) {
	int base = findSpGfxFile(0x2E);
	addSpriteTile(s,(0x8<<2)|1,base+0x02,-8,34);
	addSpriteTile(s,(0x8<<2)|0x41,base+0x02,8,34);
	addSpriteTile(s,(0x8<<2)|1,base+0x04,8,18);
	addSpriteTile(s,(0x8<<2)|1,base+0x00,0,2);
}
//Vertical log on lava
void drawSprite_01B(sprite_t * s) {
	int base = findSpGfxFile(0x48);
	addSpriteTile(s,(0xC<<2)|1,base+0x04,0,-8);
	addSpriteTile(s,(0xC<<2)|0x81,base+0x04,0,8);
}
//Dr. Freezegood
void drawSprite_01C(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x4424,-8,-8);
	addSpriteTile(s,(0x9<<2),0x4425,8,-8);
	addSpriteTile(s,(0x9<<2),0x4434,-8,8);
	addSpriteTile(s,(0x9<<2),0x4435,8,8);
}
//Dr. Freezegood with ski lift
void drawSprite_01D(sprite_t * s) {
	int base = findSpGfxFile(0x2E);
	addSpriteTile(s,(0x8<<2)|1,base+0x02,-8,34);
	addSpriteTile(s,(0x8<<2)|0x41,base+0x02,8,34);
	addSpriteTile(s,(0x8<<2)|1,base+0x04,8,18);
	addSpriteTile(s,(0x8<<2)|1,base+0x00,0,2);
	addSpriteTile(s,(0x9<<2),0x4424,-8,8);
	addSpriteTile(s,(0x9<<2),0x4425,8,8);
	addSpriteTile(s,(0x9<<2),0x4434,-8,24);
	addSpriteTile(s,(0x9<<2),0x4435,8,24);
}
//Shy-Guy
void drawSprite_01E(sprite_t * s) {
	int offset = ((s->data[2]&1)|(s->data[1]&2))<<1;
	int pal = ((romBuf[0x0209B8+offset]-1)<<1)|0x20;
	addSpriteTile(s,pal|1,0x0088,0,-3);
	addSpriteTile(s,(0x8<<2),0x009E,1,8);
	addSpriteTile(s,(0x8<<2),0x009E,7,8);
}
//Rotating door roulette
void drawSprite_01F(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x440F,-8,-48);
	addSpriteTile(s,(0x8<<2)|0x40,0x440F,8,-48);
	addSpriteTile(s,(0x8<<2),0x441F,-8,-32);
	addSpriteTile(s,(0x8<<2)|0x40,0x441F,8,-32);
	addSpriteTile(s,(0x8<<2),0x440F,-40,-16);
	addSpriteTile(s,(0x8<<2)|0x40,0x440F,-24,-16);
	addSpriteTile(s,(0x8<<2),0x441F,-40,0);
	addSpriteTile(s,(0x8<<2)|0x40,0x441F,-24,0);
	addSpriteTile(s,(0x8<<2),0x440F,-8,16);
	addSpriteTile(s,(0x8<<2)|0x40,0x440F,8,16);
	addSpriteTile(s,(0x8<<2),0x441F,-8,32);
	addSpriteTile(s,(0x8<<2)|0x40,0x441F,8,32);
	addSpriteTile(s,(0x8<<2),0x440F,24,-16);
	addSpriteTile(s,(0x8<<2)|0x40,0x440F,40,-16);
	addSpriteTile(s,(0x8<<2),0x441F,24,0);
	addSpriteTile(s,(0x8<<2)|0x40,0x441F,40,0);
	int base = findSpGfxFile(0x31);
	addSpriteTile(s,(0x9<<2)|1,base+0x0C,0,-40);
	addSpriteTile(s,(0x9<<2)|1,base+0x0E,-32,-8);
	addSpriteTile(s,(0x9<<2)|1,base+0x28,0,24);
	addSpriteTile(s,(0x9<<2)|1,base+0x2A,32,-8);
}
//Bandit
void drawSprite_020(sprite_t * s) {
	int base = findSpGfxFile(0x4E);
	addSpriteTile(s,(0x8<<2),base+0x0A,0,5);
	addSpriteTile(s,(0x8<<2),base+0x0B,8,5);
	addSpriteTile(s,(0x8<<2),base+0x1E,11,8);
	addSpriteTile(s,(0x8<<2),base+0x1E,0,8);
	addSpriteTile(s,(0x8<<2)|1,base+0x08,0,-10);
}
//Bucket
void drawSprite_021(sprite_t * s) {
	addSpriteTile(s,(0xA<<2),0x412C,-8,-8);
	addSpriteTile(s,(0xA<<2),0x412D,8,-8);
	addSpriteTile(s,(0xA<<2),0x413C,-8,8);
	addSpriteTile(s,(0xA<<2),0x413D,8,8);
}
//Egg
void drawSprite_022(sprite_t * s) {
	int spRef = (s->data[0]-0x22)<<2;
	int pal = (0xB<<2)-spRef;
	addSpriteTile(s,pal|1,0x0082,0,0);
}
//Giant egg from final boss
void drawSprite_026(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|1,0x0187,4,-8);
	addSpriteTile(s,(0x9<<2)|1,0x0185,-4,0);
	addSpriteTile(s,(0x9<<2),0x0197,12,8);
	addSpriteTile(s,(0x9<<2),0x0194,-4,-8);
}
//Key
void drawSprite_027(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|1,0x00EE,0,0);
}
//Huffin' Puffin
void drawSprite_028(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|1,0x0102,0,-4);
	addSpriteTile(s,(0x9<<2),0x009E,4,8);
}
//Giant egg which follows Superstar Mario
void drawSprite_029(sprite_t * s) {
	addSpriteTile(s,(0xD<<2),0x25B0,-7,-15);
	addSpriteTile(s,(0xD<<2),0x25B1,1,-15);
	addSpriteTile(s,(0xD<<2),0x25B8,-7,-7);
	addSpriteTile(s,(0xD<<2),0x25B9,1,-7);
	addSpriteTile(s,(0xD<<2),0x25B2,7,-15);
	addSpriteTile(s,(0xD<<2),0x25B3,15,-15);
	addSpriteTile(s,(0xD<<2),0x25BA,7,-7);
	addSpriteTile(s,(0xD<<2),0x25BB,15,-7);
	addSpriteTile(s,(0xD<<2),0x25B4,-7,0);
	addSpriteTile(s,(0xD<<2),0x25B5,1,0);
	addSpriteTile(s,(0xD<<2),0x25BC,-7,8);
	addSpriteTile(s,(0xD<<2),0x25BD,1,8);
	addSpriteTile(s,(0xD<<2),0x25B6,7,0);
	addSpriteTile(s,(0xD<<2),0x25B7,15,0);
	addSpriteTile(s,(0xD<<2),0x25BE,7,8);
	addSpriteTile(s,(0xD<<2),0x25BF,15,8);
}
//Giant egg
void drawSprite_02A(sprite_t * s) {
	int spRef = (s->data[0]-0x2A)<<2;
	int pal = ((0x9<<2)-spRef)|1;
	int base = findSpGfxFile(0x2F);
	addSpriteTile(s,pal,base+0x06,-6,-14);
	addSpriteTile(s,pal,base+0x0A,6,-14);
	addSpriteTile(s,pal,base+0x0C,-6,0);
	addSpriteTile(s,pal,base+0x0E,6,0);
}
//Lunge Fish
void drawSprite_02C(sprite_t * s) {
	int base = findSpGfxFile(0x32);
	addSpriteTile(s,(0x9<<2)|1,base+0x4A,16,16);
	addSpriteTile(s,(0x9<<2)|1,base+0x48,0,16);
	addSpriteTile(s,(0x9<<2)|1,base+0x46,-16,16);
	addSpriteTile(s,(0x9<<2)|1,base+0x2A,16,0);
	addSpriteTile(s,(0x9<<2)|1,base+0x28,0,0);
	addSpriteTile(s,(0x9<<2)|1,base+0x26,-16,0);
	addSpriteTile(s,(0x9<<2)|1,base+0x0A,16,-16);
	addSpriteTile(s,(0x9<<2)|1,base+0x08,0,-16);
	addSpriteTile(s,(0x9<<2)|1,base+0x06,-16,-16);
}
//Salvo
void drawSprite_02D(sprite_t * s) {
	for(int i=0; i<96; i++) {
		int width = romBuf[0x05060F+i];
		addSpriteTile(s,0,0x8000+width,-0x80,i-96);
	}
	int base = findSpGfxFile(0x45);
	addSpriteTile(s,(0x8<<2),base+0x0D,-40,-56);
	addSpriteTile(s,(0x8<<2),base+0x0D,-32,-56);
	addSpriteTile(s,(0x8<<2),base+0x1D,-40,-48);
	addSpriteTile(s,(0x8<<2),base+0x1D,-32,-48);
}
//Salvo eyes
void drawSprite_02E(sprite_t * s) {
	int base = findSpGfxFile(0x45);
	addSpriteTile(s,(0x8<<2),base+0x0D,0,0);
	addSpriteTile(s,(0x8<<2),base+0x0D,8,0);
	addSpriteTile(s,(0x8<<2),base+0x1D,0,8);
	addSpriteTile(s,(0x8<<2),base+0x1D,8,8);
}
//Little Mouser nest
void drawSprite_02F(sprite_t * s) {
	int base = findSpGfxFile(0x28);
	addSpriteTile(s,(0xE<<2)|1,base+0x06,0,0);
}
//Little Mouser
void drawSprite_030(sprite_t * s) {
	int base = findSpGfxFile(0x28);
	addSpriteTile(s,(0xE<<2),base+0x02,0,8);
	addSpriteTile(s,(0xE<<2)|1,base+0x00,-2,-1);
	addSpriteTile(s,(0xE<<2),base+0x14,14,4);
	addSpriteTile(s,(0xE<<2),base+0x02,8,8);
}
//Potted Spiked Fun Guy
void drawSprite_031(sprite_t * s) {
	int base = findSpGfxFile(0x36);
	addSpriteTile(s,(0x8<<2)|0x41,base+0x08,0,-10);
	addSpriteTile(s,(0xC<<2)|1,base+0x0E,0,0);
}
//Roger the Potted Ghost
void drawSprite_034(sprite_t * s) {
	drawSpriteHDMAPolygon(s,&romBuf[0x010E7D],0x8000,66,false,false,0,0);
	int base = findSpGfxFile(0x42);
	addSpriteTile(s,(0x9<<2)|1,base+0x20,-5,-86);
	addSpriteTile(s,(0x9<<2)|1,base+0x22,11,-86);
	addSpriteTile(s,(0x9<<2)|1,base+0x02,11,-102);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,-5,-102);
	addSpriteTile(s,(0x9<<2),base+0x3B,1,-111);
	addSpriteTile(s,(0x9<<2),base+0x3B,11,-111);
	addSpriteTile(s,(0x9<<2)|1,base+0x24,-5,-120);
	addSpriteTile(s,(0x9<<2)|1,base+0x26,8,-120);
	addSpriteTile(s,(0x9<<2)|1,base+0x44,6,-143);
	addSpriteTile(s,(0x9<<2)|1,base+0x42,-10,-143);
	addSpriteTile(s,(0x9<<2)|1,base+0x40,-2,-159);
	if(s->data[0]==0x34) {
		addSpriteTile(s,(0x9<<2)|1,base+0x49,4,0);
		addSpriteTile(s,(0x9<<2)|1,base+0x48,-4,0);
		addSpriteTile(s,(0x9<<2),base+0x4F,12,-8);
		addSpriteTile(s,(0x9<<2),base+0x4E,4,-8);
		addSpriteTile(s,(0x9<<2),base+0x4D,-4,-8);
	}
}
//Fake falling wall
void drawSprite_036(sprite_t * s) {
	for(int j=-96; j<16; j++) {
		addSpriteTile(s,0,0x855F,-0x50,j);
	}
}
//Grim Leecher
void drawSprite_037(sprite_t * s) {
	int base = findSpGfxFile(0x46);
	addSpriteTile(s,(0xA<<2)|1,base+0x0A,0,0);
}
//Roger the Potted Ghost projectile
void drawSprite_038(sprite_t * s) {
	int base = findSpGfxFile(0x42);
	addSpriteTile(s,(0xB<<2)|1,base+0x4B,0,0);
}
//Spinning platform
void drawSprite_039(sprite_t * s) {
	for(int j=0; j<48; j++) {
		addSpriteTile(s,0,0x84D0,-0x70,j);
	}
}
//3 Mini Ravens
void drawSprite_03A(sprite_t * s) {
	int base = findSpGfxFile(0x4F);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,-18,-2);
	addSpriteTile(s,(0x9<<2),base+0x12,-13,-1);
	addSpriteTile(s,(0x9<<2),base+0x02,-18,3);
	addSpriteTile(s,(0x9<<2),base+0x16,-5,-5);
	addSpriteTile(s,(0x9<<2),base+0x05,-16,13);
	addSpriteTile(s,(0x9<<2)|0x40,base+0x05,-8,13);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,-2,-2);
	addSpriteTile(s,(0x9<<2),base+0x12,3,-1);
	addSpriteTile(s,(0x9<<2),base+0x02,-2,3);
	addSpriteTile(s,(0x9<<2),base+0x16,11,-5);
	addSpriteTile(s,(0x9<<2),base+0x05,0,13);
	addSpriteTile(s,(0x9<<2)|0x40,base+0x05,8,13);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,14,-2);
	addSpriteTile(s,(0x9<<2),base+0x12,19,-1);
	addSpriteTile(s,(0x9<<2),base+0x02,14,3);
	addSpriteTile(s,(0x9<<2),base+0x16,27,-5);
	addSpriteTile(s,(0x9<<2),base+0x05,16,13);
	addSpriteTile(s,(0x9<<2)|0x40,base+0x05,24,13);
}
//Mini Raven
void drawSprite_03B(sprite_t * s) {
	int base = findSpGfxFile(0x4F);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,-2,-2);
	addSpriteTile(s,(0x9<<2),base+0x12,3,-1);
	addSpriteTile(s,(0x9<<2),base+0x02,-2,3);
	addSpriteTile(s,(0x9<<2),base+0x16,11,-5);
	addSpriteTile(s,(0x9<<2),base+0x05,0,13);
	addSpriteTile(s,(0x9<<2)|0x40,base+0x05,8,13);
}
//Tap-Tap the Red Nose
void drawSprite_03C(sprite_t * s) {
	for(int j=0; j<4; j++) {
		for(int i=0; i<4; i++) {
			int offsX = -24+(i<<4);
			int offsY = -32+(j<<4);
			addSpriteTile(s,(0xE<<2),0x450C+i+(j<<4),offsX,offsY);
		}
	}
	addSpriteTile(s,(0xE<<2),0x4548,-14,-24);
	addSpriteTile(s,(0xE<<2),0x4549,2,-24);
	addSpriteTile(s,(0xE<<2),0x4558,-14,-8);
	addSpriteTile(s,(0xE<<2),0x4559,2,-8);
	addSpriteTile(s,(0xE<<2),0x456A,-25,10);
	addSpriteTile(s,(0xE<<2),0x456B,-9,10);
	addSpriteTile(s,(0xE<<2),0x457A,-25,26);
	addSpriteTile(s,(0xE<<2),0x457B,-9,26);
	addSpriteTile(s,(0xE<<2),0x456A,9,10);
	addSpriteTile(s,(0xE<<2),0x456B,25,10);
	addSpriteTile(s,(0xE<<2),0x457A,9,26);
	addSpriteTile(s,(0xE<<2),0x457B,25,26);
	addSpriteTile(s,(0xE<<2)|1,0x010E,-21,-17);
}
//Seesaw
void drawSprite_03D(sprite_t * s) {
	drawSpriteHDMAPolygon(s,&romBuf[0x023330],0x8400,16,false,false,0x80,0x10);
}
//Skinny platform
void drawSprite_03E(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x0066,-32,0);
	addSpriteTile(s,(0x9<<2),0x0067,-24,0);
	addSpriteTile(s,(0x9<<2),0x0067,-16,0);
	addSpriteTile(s,(0x9<<2),0x0067,-8,0);
	addSpriteTile(s,(0x9<<2),0x0067,0,0);
	addSpriteTile(s,(0x9<<2),0x0067,8,0);
	addSpriteTile(s,(0x9<<2),0x0067,16,0);
	addSpriteTile(s,(0x9<<2)|0x40,0x0066,24,0);
}
//Slime
void drawSprite_03F(sprite_t * s) {
	drawSpriteHDMAPolygon(s,&romBuf[0x030D80],0x8400,64,false,false,0,0);
	addSpriteTile(s,(0x8<<2),0x012D,-40,-56);
	addSpriteTile(s,(0x8<<2),0x012D,-32,-56);
	addSpriteTile(s,(0x8<<2),0x013D,-40,-48);
	addSpriteTile(s,(0x8<<2),0x013D,-32,-48);
}
//Baby Luigi
void drawSprite_040(sprite_t * s) {
	addSpriteTile(s,(0xE<<2),0x40AA,-8,-10);
	addSpriteTile(s,(0xE<<2)|1,0x01A4,0,-14);
}
//Stork
void drawSprite_041(sprite_t * s) {
	addSpriteTile(s,(0xE<<2)|0x40,0x019D,-27,72);
	addSpriteTile(s,(0xE<<2)|0x41,0x018B,-19,64);
	addSpriteTile(s,(0xE<<2)|0x41,0x018E,-24,80);
	addSpriteTile(s,(0xE<<2)|1,0x0187,-8,55);
	addSpriteTile(s,(0xE<<2)|1,0x0189,-8,40);
	addSpriteTile(s,(0xE<<2)|1,0x014C,-12,71);
	addSpriteTile(s,(0xE<<2)|1,0x018B,3,64);
	addSpriteTile(s,(0xE<<2)|1,0x018E,8,80);
	addSpriteTile(s,(0xE<<2),0x019D,21,72);
	addSpriteTile(s,(0xE<<2)|0x80,0x0190,-20,80);
	addSpriteTile(s,(0xE<<2),0x018D,-4,16);
	addSpriteTile(s,(0xE<<2),0x018D,-4,24);
	addSpriteTile(s,(0xE<<2),0x018D,-4,32);
	addSpriteTile(s,(0xE<<2)|1,0x014A,-8,0);
}
//Vertical pipe entrance
void drawSprite_042(sprite_t * s) {
	drawSpriteText(s,"Vertical Pipe\n  Exit Down  ");
}
//Giant Shy-Guy
void drawSprite_043(sprite_t * s) {
	int spRef = (s->data[0]-0x43)<<2;
	int pal = ((0x9<<2)-spRef)|1;
	int base = findSpGfxFile(0x70);
	addSpriteTile(s,pal|1,base+0x00,-8,6);
	addSpriteTile(s,pal|1,base+0x02,8,6);
	addSpriteTile(s,pal|1,base+0x04,-8,-10);
	addSpriteTile(s,pal|1,base+0x06,8,-10);
	addSpriteTile(s,pal|0x40,base+0x1A,-9,12);
	addSpriteTile(s,pal|0x40,base+0x0A,-11,8);
	addSpriteTile(s,pal,base+0x1A,16,12);
	addSpriteTile(s,pal,base+0x0A,18,8);
}
//Prince Froggy
void drawSprite_045(sprite_t * s) {
	int offsY = (s->data[1]&2);
	if(s->data[2]&1) {
		int base = findSpGfxFile(0x68);
		if(offsY) {
			addSpriteTile(s,(0x9<<2)|0x40,base+0x0C,43,29);
			addSpriteTile(s,(0x9<<2)|0xC0,base+0x19,22,9);
			addSpriteTile(s,(0x9<<2)|0xC0,base+0x19,31,7);
			addSpriteTile(s,(0x9<<2)|0x40,base+0x03,48,26);
			addSpriteTile(s,(0x9<<2)|0x80,base+0x19,41,12);
			addSpriteTile(s,(0x9<<2),base+0x09,36,12);
			addSpriteTile(s,(0x9<<2)|0xC0,base+0x04,40,25);
			addSpriteTile(s,(0x9<<2)|0xC0,base+0x05,32,22);
			addSpriteTile(s,(0x9<<2)|0xC0,base+0x12,32,15);
			addSpriteTile(s,(0x9<<2),base+0x16,41,18);
			addSpriteTile(s,(0x9<<2)|0xC0,base+0x14,40,16);
			addSpriteTile(s,(0x9<<2),base+0x09,26,13);
			addSpriteTile(s,(0x9<<2),base+0x0C,36,27);
			addSpriteTile(s,(0x9<<2)|0xC0,base+0x19,31,14);
			addSpriteTile(s,(0x9<<2),base+0x0D,46,16);
		} else {
			addSpriteTile(s,(0x9<<2),base+0x08,-2,2);
			addSpriteTile(s,(0x9<<2),base+0x18,-2,10);
			addSpriteTile(s,(0x9<<2)|1,base+0x00,-8,0);
			addSpriteTile(s,(0x9<<2)|1,base+0x01,0,0);
			addSpriteTile(s,(0x9<<2),base+0x0C,-3,-5);
			addSpriteTile(s,(0x9<<2),base+0x0C,4,-5);
			addSpriteTile(s,(0x9<<2),base+0x08,14,2);
			addSpriteTile(s,(0x9<<2),base+0x18,14,10);
			addSpriteTile(s,(0x9<<2),base+0x06,5,10);
			addSpriteTile(s,(0x9<<2),base+0x06,-4,10);
		}
	} else {
		BYTE tempBuf[128*2];
		for(int n=0; n<16; n++) {
			int px0 = romBuf[0x014774+(n<<1)];
			int py0 = romBuf[0x014775+(n<<1)];
			int px1 = romBuf[0x014774+(((n+1)&0xF)<<1)];
			int py1 = romBuf[0x014775+(((n+1)&0xF)<<1)];
			int px2 = romBuf[0x014774+(((n+2)&0xF)<<1)];
			int py2 = romBuf[0x014775+(((n+2)&0xF)<<1)];
			if(px0&0x80) px0 -= 0x100;
			if(py0&0x80) py0 -= 0x100;
			if(px1&0x80) px1 -= 0x100;
			if(py1&0x80) py1 -= 0x100;
			if(px2&0x80) px2 -= 0x100;
			if(py2&0x80) py2 -= 0x100;
			for(int m=0; m<8; m++) {
				int offset = ((n<<3)+m)<<1;
				int c0 = (8-m)*(8-m);
				int c2 = m*m;
				int c1 = 128-c0-c2;
				tempBuf[offset] = ((px0*c0)+(px1*c1)+(px2*c2))>>7;
				tempBuf[offset+1] = ((py0*c0)+(py1*c1)+(py2*c2))>>7;
			}
		}
		drawSpriteHDMAPolygon(s,tempBuf,0x8000,128,false,false,0,0);
		if(offsY) {
			int base = findSpGfxFile(0x70);
			addSpriteTile(s,(0xF<<2),base+0x0B,-8,-80);
			addSpriteTile(s,(0xF<<2)|0x40,base+0x0B,0,-80);
			addSpriteTile(s,(0xF<<2),0x4664,-16,-77);
			addSpriteTile(s,(0xF<<2),0x4665,0,-77);
			addSpriteTile(s,(0xF<<2),0x4674,-16,-61);
			addSpriteTile(s,(0xF<<2),0x4675,0,-61);
		}
	}
}
//Burt the Bashful
void drawSprite_046(sprite_t * s) {
	addSpriteTile(s,(0xE<<2),0x4263,17,-122);
	for(int j=0; j<64; j++) {
		int row = romBuf[0x050A51+j];
		addSpriteTile(s,0,0x8200|row,-0x80,j-0x78);
	}
	addSpriteTile(s,(0xE<<2),0x4264,0,-108);
	addSpriteTile(s,(0xE<<2),0x4265,16,-108);
	addSpriteTile(s,(0xE<<2),0x4274,0,-92);
	addSpriteTile(s,(0xE<<2),0x4275,16,-92);
	addSpriteTile(s,(0xE<<2),0x4273,17,-64);
	addSpriteTile(s,(0xF<<2)|0x40,0x4273,33,-64);
	addSpriteTile(s,(0xE<<2),0x4272,17,-48);
	addSpriteTile(s,(0xF<<2)|0x40,0x4272,33,-48);
	for(int j=0; j<64; j++) {
		int row = romBuf[0x050A90+j];
		addSpriteTile(s,0,0x8100|row,-0x80,j-0x38);
	}
	addSpriteTile(s,(0xE<<2),0x4262,-18,-122);
	addSpriteTile(s,(0xE<<2),0x4262,-3,-122);
	addSpriteTile(s,(0xE<<2),0x4260,-65,-118);
	addSpriteTile(s,(0xE<<2),0x4261,-49,-118);
	addSpriteTile(s,(0xE<<2),0x4270,-65,-102);
	addSpriteTile(s,(0xE<<2),0x4271,-49,-102);
	addSpriteTile(s,(0xF<<2)|0xC0,0x4260,-25,-102);
	addSpriteTile(s,(0xF<<2)|0xC0,0x4261,-41,-102);
	addSpriteTile(s,(0xF<<2)|0xC0,0x4270,-25,-118);
	addSpriteTile(s,(0xF<<2)|0xC0,0x4271,-41,-118);
	addSpriteTile(s,(0xE<<2)|0x80,0x4260,-56,8);
	addSpriteTile(s,(0xE<<2)|0x80,0x4261,-40,8);
	addSpriteTile(s,(0xE<<2)|0x80,0x4270,-56,-8);
	addSpriteTile(s,(0xE<<2)|0x80,0x4271,-40,-8);
	addSpriteTile(s,(0xF<<2)|0x40,0x4260,-16,-8);
	addSpriteTile(s,(0xF<<2)|0x40,0x4261,-32,-8);
	addSpriteTile(s,(0xF<<2)|0x40,0x4270,-16,8);
	addSpriteTile(s,(0xF<<2)|0x40,0x4271,-32,8);
	addSpriteTile(s,(0xE<<2)|0xC0,0x4260,40,8);
	addSpriteTile(s,(0xE<<2)|0xC0,0x4261,24,8);
	addSpriteTile(s,(0xE<<2)|0xC0,0x4270,40,-8);
	addSpriteTile(s,(0xE<<2)|0xC0,0x4271,24,-8);
	addSpriteTile(s,(0xF<<2),0x4260,0,-8);
	addSpriteTile(s,(0xF<<2),0x4261,16,-8);
	addSpriteTile(s,(0xF<<2),0x4270,0,8);
	addSpriteTile(s,(0xF<<2),0x4271,16,8);
}
//Shy Guy from Roger the Potted Ghost
void drawSprite_047(sprite_t * s) {
	addSpriteTile(s,(0x8<<2)|1,0x012C,-1,-3);
	addSpriteTile(s,(0x8<<2),0x010B,9,-8);
	addSpriteTile(s,(0x8<<2),0x010C,8,8);
}
//Kamek cutscene
void drawSprite_048(sprite_t * s) {
	int base = findSpGfxFile(0x6A);
	addSpriteTile(s,(0x9<<2)|1,base+0x0E,16,4);
	addSpriteTile(s,(0x9<<2),base+0x1A,9,10);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,-9,-8);
	addSpriteTile(s,(0x9<<2)|1,base+0x02,-13,0);
	addSpriteTile(s,(0x9<<2)|1,base+0x04,3,0);
	addSpriteTile(s,(0x9<<2)|0x40,base+0x10,5,-8);
	addSpriteTile(s,(0x9<<2),base+0x0A,-5,8);
}
//Fire from Thunder Lakitu
void drawSprite_049(sprite_t * s) {
	addSpriteTile(s,(0xA<<2)|0x41,0x01AE,0,0);
	addSpriteTile(s,(0xA<<2)|0x41,0x01AC,0,-16);
}
//Upside-down Blow Hard
void drawSprite_04C(sprite_t * s) {
	int base = findSpGfxFile(0x36);
	addSpriteTile(s,(8<<2)|0x81,base+0x00,-8,0);
	addSpriteTile(s,(8<<2)|0x81,base+0x02,8,0);
	addSpriteTile(s,(8<<2)|0x80,0x4122,-16,20);
	addSpriteTile(s,(8<<2)|0x80,0x4123,0,20);
	addSpriteTile(s,(8<<2)|0x80,0x4132,-16,4);
	addSpriteTile(s,(8<<2)|0x80,0x4133,0,4);
}
//Unused sprite
void drawSprite_04D(sprite_t * s) {
	addSpriteTile(s,(0x8<<2)|0x41,0x0182,-8,-8);
	addSpriteTile(s,(0x8<<2)|0x41,0x0180,8,-8);
	addSpriteTile(s,(0x8<<2)|0x41,0x01A2,-8,8);
	addSpriteTile(s,(0x8<<2)|0x41,0x01A0,8,8);
}
//Locked door
void drawSprite_04E(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x4160,-8,0);
	addSpriteTile(s,(0x8<<2)|0x40,0x4160,8,0);
	addSpriteTile(s,(0x8<<2),0x4170,-8,16);
	addSpriteTile(s,(0x8<<2)|0x40,0x4170,8,16);
}
//Middle Ring
void drawSprite_04F(sprite_t * s) {
	addSpriteTile(s,(0xB<<2),0x0055,8,39);
	addSpriteTile(s,(0xB<<2),0x0045,1,35);
	addSpriteTile(s,(0xB<<2),0x0054,-5,27);
	addSpriteTile(s,(0xB<<2),0x0044,-8,14);
	addSpriteTile(s,(0xB<<2),0x0054,-8,0);
	addSpriteTile(s,(0xB<<2),0x0045,-5,-13);
	addSpriteTile(s,(0xB<<2),0x0055,0,-22);
	addSpriteTile(s,(0xB<<2),0x0045,8,-25);
	addSpriteTile(s,(0xB<<2),0x0054,14,-22);
	addSpriteTile(s,(0xB<<2),0x0044,20,-14);
	addSpriteTile(s,(0xB<<2),0x0054,23,-1);
	addSpriteTile(s,(0xB<<2),0x0045,23,14);
	addSpriteTile(s,(0xB<<2),0x0055,20,26);
	addSpriteTile(s,(0xB<<2),0x0045,15,35);
}
//Wide plank
void drawSprite_050(sprite_t * s) {
	for(int j=0; j<16; j++) {
		addSpriteTile(s,0,0x845A,-0x58,j);
	}
}
//Octagonal log
void drawSprite_051(sprite_t * s) {
	int x = 95<<16;
	for(int j=-55; j<-45; j++) {
		int width = x>>16;
		x += (16<<16)/10;
		addSpriteTile(s,0,0x8400+width,-0x58,j+0x10);
	}
	x = 111<<16;
	for(int j=-45; j<0; j++) {
		int width = x>>16;
		x += (9<<16)/45;
		addSpriteTile(s,0,0x8500+width,-0x58,j+0x10);
	}
	x = 120<<16;
	for(int j=0; j<45; j++) {
		int width = x>>16;
		x -= (9<<16)/45;
		addSpriteTile(s,0,0x8480+width,-0x58,j+0x10);
	}
	x = 111<<16;
	for(int j=45; j<55; j++) {
		int width = x>>16;
		x -= (16<<16)/10;
		addSpriteTile(s,0,0x8580+width,-0x58,j+0x10);
	}
}
//Balloon platform, randomly colored
void drawSprite_052(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x4504,-8,-8);
	addSpriteTile(s,(0x9<<2),0x4505,8,-8);
	addSpriteTile(s,(0x9<<2),0x4514,-8,8);
	addSpriteTile(s,(0x9<<2),0x4515,8,8);
}
//Upside-down Wild Piranha
void drawSprite_054(sprite_t * s) {
	int base = findSpGfxFile(0x29);
	addSpriteTile(s,(0x9<<2)|0x80,base+0x1A,0,0);
	addSpriteTile(s,(0x9<<2)|0xC0,base+0x1A,8,0);
	addSpriteTile(s,(0x9<<2)|0x80,base+0x0A,4,8);
	addSpriteTile(s,(0x9<<2)|0x80,0x446C,-23,20);
	addSpriteTile(s,(0x9<<2)|0x80,0x446D,-7,20);
	addSpriteTile(s,(0x9<<2)|0x80,0x447C,-23,4);
	addSpriteTile(s,(0x9<<2)|0x80,0x447D,-7,4);
}
//Flatbed Ferry pinwheel, green
void drawSprite_055(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x00BD,4,-4);
	addSpriteTile(s,(0x8<<2),0x00BD,8,-16);
	addSpriteTile(s,(0x8<<2),0x00BD,12,-28);
	addSpriteTile(s,(0x8<<2),0x00BD,16,0);
	addSpriteTile(s,(0x8<<2),0x00BD,28,4);
	addSpriteTile(s,(0x8<<2),0x00BD,0,8);
	addSpriteTile(s,(0x8<<2),0x00BD,-4,20);
	addSpriteTile(s,(0x8<<2),0x00BD,-8,-8);
	addSpriteTile(s,(0x8<<2),0x00BD,-20,-12);
	addSpriteTile(s,(0x8<<2)|1,0x0028,7,-45);
	addSpriteTile(s,(0x8<<2)|1,0x002A,23,-45);
	addSpriteTile(s,(0x8<<2)|1,0x0028,29,7);
	addSpriteTile(s,(0x8<<2)|1,0x002A,45,7);
	addSpriteTile(s,(0x8<<2)|1,0x0028,-23,29);
	addSpriteTile(s,(0x8<<2)|1,0x002A,-7,29);
	addSpriteTile(s,(0x8<<2)|1,0x0028,-45,-23);
	addSpriteTile(s,(0x8<<2)|1,0x002A,-29,-23);
}
//Flatbed Ferry pinwheel, pink
void drawSprite_056(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x00BD,4,-4);
	addSpriteTile(s,(0x8<<2),0x00BD,8,-16);
	addSpriteTile(s,(0x8<<2),0x00BD,16,0);
	addSpriteTile(s,(0x8<<2),0x00BD,0,8);
	addSpriteTile(s,(0x8<<2),0x00BD,-8,-8);
	addSpriteTile(s,(0xC<<2)|1,0x0028,5,-30);
	addSpriteTile(s,(0xC<<2)|1,0x002A,13,-30);
	addSpriteTile(s,(0xC<<2)|1,0x0028,18,1);
	addSpriteTile(s,(0xC<<2)|1,0x002A,26,1);
	addSpriteTile(s,(0xC<<2)|1,0x0028,-13,14);
	addSpriteTile(s,(0xC<<2)|1,0x002A,-5,14);
	addSpriteTile(s,(0xC<<2)|1,0x0028,-26,-17);
	addSpriteTile(s,(0xC<<2)|1,0x002A,-18,-17);
}
//Roger Lift
void drawSprite_057(sprite_t * s) {
	drawSpriteHDMAPolygon(s,&romBuf[0x0378BB],0x8400,50,true,true,-0x80,-0xD8);
	addSpriteTile(s,(0x8<<2)|1,0x0028,76,-232);
	addSpriteTile(s,(0x8<<2)|1,0x0029,92,-232);
	addSpriteTile(s,(0x8<<2)|1,0x002A,108,-232);
	addSpriteTile(s,(0x8<<2)|0x40,0x010D,90,-224);
	addSpriteTile(s,(0x8<<2)|0x40,0x010D,106,-224);
	addSpriteTile(s,(0x8<<2)|0x40,0x011D,90,-216);
	addSpriteTile(s,(0x8<<2)|0x40,0x011D,106,-216);
}
//Solo Toady
void drawSprite_058(sprite_t * s) {
	int spRef = (s->data[0]-0x58)<<2;
	int pal = (0x8<<2)+spRef;
	addSpriteTile(s,pal,0x002C,0,-6);
	addSpriteTile(s,pal|0x40,0x002C,8,-6);
	addSpriteTile(s,pal|0x40,0x002F,6,14);
	addSpriteTile(s,pal|0x40,0x002F,1,14);
	addSpriteTile(s,pal|1,0x000E,0,0);
}
//Super Star
void drawSprite_059(sprite_t * s) {
	addSpriteTile(s,(0xA<<2),0x416F,0,0);
}
//Raphael Raven projectile
void drawSprite_05A(sprite_t * s) {
	int base = findSpGfxFile(0x1B);
	addSpriteTile(s,(9<<2)|1,base+0x20,-8,-8);
	addSpriteTile(s,(9<<2)|1,base+0x22,8,-8);
	addSpriteTile(s,(9<<2)|1,base+0x24,-8,8);
	addSpriteTile(s,(9<<2)|0x41,base+0x24,8,8);
}
//Coin Bandit
void drawSprite_05B(sprite_t * s) {
	int base = findSpGfxFile(0x4E);
	addSpriteTile(s,(0xC<<2),base+0x0A,0,5);
	addSpriteTile(s,(0xC<<2),base+0x0B,8,5);
	addSpriteTile(s,(0xC<<2),base+0x1E,11,8);
	addSpriteTile(s,(0xC<<2),base+0x1E,0,8);
	addSpriteTile(s,(0xC<<2)|1,base+0x08,0,-10);
	addSpriteTile(s,(0x9<<2)|1,0x00A0,0,-26);
}
//Short plank
void drawSprite_05E(sprite_t * s) {
	for(int j=0; j<16; j++) {
		addSpriteTile(s,0,0x8434,-0x78,j);
	}
}
//Bomb
void drawSprite_060(sprite_t * s) {
	addSpriteTile(s,(0xC<<2)|1,0x00EC,0,0);
}
//Baby Mario
void drawSprite_061(sprite_t * s) {
	addSpriteTile(s,(0xD<<2)|1,0x0064,0,3);
	addSpriteTile(s,(0xD<<2)|1,0x0062,1,-6);
}
//Goomba
void drawSprite_062(sprite_t * s) {
	int base = findSpGfxFile(0x26);
	addSpriteTile(s,(0xC<<2)|1,base+0x0C,0,-1);
	addSpriteTile(s,(0x9<<2),base+0x0E,2,12);
	addSpriteTile(s,(0x9<<2)|0x40,base+0x0E,7,12);
	addSpriteTile(s,(0xC<<2),base+0x1F,4,-6);
}
//Muddy Buddy
void drawSprite_063(sprite_t * s) {
	addSpriteTile(s,(0xC<<2),0x009E,8,9);
	addSpriteTile(s,(0xC<<2),0x009E,0,9);
	addSpriteTile(s,(0xC<<2),0x4040,0,-1);
}
//Flatbed Ferry pinwheel, pink
void drawSprite_064(sprite_t * s) {
	if(s->data[1]&2) {
		addSpriteTile(s,(0x8<<2),0x00BD,4,-4);
		addSpriteTile(s,(0x8<<2),0x00BD,8,-16);
		addSpriteTile(s,(0x8<<2),0x00BD,16,0);
		addSpriteTile(s,(0x8<<2),0x00BD,0,8);
		addSpriteTile(s,(0x8<<2),0x00BD,-8,-8);
		addSpriteTile(s,(0xC<<2)|1,0x0028,5,-30);
		addSpriteTile(s,(0xC<<2)|1,0x002A,13,-30);
		addSpriteTile(s,(0xC<<2)|1,0x0028,18,1);
		addSpriteTile(s,(0xC<<2)|1,0x002A,26,1);
		addSpriteTile(s,(0xC<<2)|1,0x0028,-13,14);
		addSpriteTile(s,(0xC<<2)|1,0x002A,-5,14);
		addSpriteTile(s,(0xC<<2)|1,0x0028,-26,-17);
		addSpriteTile(s,(0xC<<2)|1,0x002A,-18,-17);
	} else {
		addSpriteTile(s,(0x8<<2),0x00BD,4,-4);
		addSpriteTile(s,(0x8<<2),0x00BD,8,-16);
		addSpriteTile(s,(0x8<<2),0x00BD,12,-28);
		addSpriteTile(s,(0x8<<2),0x00BD,16,0);
		addSpriteTile(s,(0x8<<2),0x00BD,28,4);
		addSpriteTile(s,(0x8<<2),0x00BD,0,8);
		addSpriteTile(s,(0x8<<2),0x00BD,-4,20);
		addSpriteTile(s,(0x8<<2),0x00BD,-8,-8);
		addSpriteTile(s,(0x8<<2),0x00BD,-20,-12);
		addSpriteTile(s,(0xC<<2)|1,0x0028,7,-45);
		addSpriteTile(s,(0xC<<2)|1,0x002A,23,-45);
		addSpriteTile(s,(0xC<<2)|1,0x0028,29,7);
		addSpriteTile(s,(0xC<<2)|1,0x002A,45,7);
		addSpriteTile(s,(0xC<<2)|1,0x0028,-23,29);
		addSpriteTile(s,(0xC<<2)|1,0x002A,-7,29);
		addSpriteTile(s,(0xC<<2)|1,0x0028,-45,-23);
		addSpriteTile(s,(0xC<<2)|1,0x002A,-29,-23);
	}
}
//Red coin
void drawSprite_065(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|1,0x00A0,0,0);
}
//Wild Piranha
void drawSprite_066(sprite_t * s) {
	int base = findSpGfxFile(0x29);
	addSpriteTile(s,(0x9<<2),base+0x1A,0,8);
	addSpriteTile(s,(0x9<<2)|0x40,base+0x1A,8,8);
	addSpriteTile(s,(0x9<<2),base+0x0A,4,0);
	addSpriteTile(s,(0x9<<2),0x446C,-23,-20);
	addSpriteTile(s,(0x9<<2),0x446D,-7,-20);
	addSpriteTile(s,(0x9<<2),0x447C,-23,-4);
	addSpriteTile(s,(0x9<<2),0x447D,-7,-4);
}
//Winged cloud
void drawSprite_067(sprite_t * s) {
	addSpriteTile(s,(0x8<<2)|0x40,0x00D7,-7,2);
	addSpriteTile(s,(0x8<<2)|0x40,0x00C7,-10,-1);
	addSpriteTile(s,(0x8<<2),0x00D7,15,2);
	addSpriteTile(s,(0x8<<2),0x00C7,18,-1);
	addSpriteTile(s,(0x8<<2)|1,0x00CC,0,0);
}
//Flashing egg block
void drawSprite_068(sprite_t * s) {
	addSpriteTile(s,(0xB<<2)|1,0x00C0,0,0);
}
//Red egg block
void drawSprite_069(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|1,0x00C0,0,0);
}
//Yellow egg block
void drawSprite_06A(sprite_t * s) {
	addSpriteTile(s,(0xA<<2)|1,0x00C0,0,0);
}
//Hit green egg block
void drawSprite_06B(sprite_t * s) {
	addSpriteTile(s,(0x8<<2)|1,0x4074,0,0);
}
//Large spring ball
void drawSprite_06C(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x414E,-8,-8);
	addSpriteTile(s,(0x8<<2),0x414F,8,-8);
	addSpriteTile(s,(0x8<<2),0x415E,-8,8);
	addSpriteTile(s,(0x8<<2),0x415F,8,8);
}
//Propeller Piranha
void drawSprite_06D(sprite_t * s) {
	addSpriteTile(s,(0xA<<2),0x4446,-8,-8);
	addSpriteTile(s,(0xA<<2),0x4447,8,-8);
	addSpriteTile(s,(0xA<<2),0x4456,-8,8);
	addSpriteTile(s,(0xA<<2),0x4457,8,8);
}
//Spring ball
void drawSprite_06F(sprite_t * s) {
	addSpriteTile(s,(0x8<<2)|1,0x00C2,0,0);
}
//Clawdaddy
void drawSprite_070(sprite_t * s) {
	int base = findSpGfxFile(0x35);
	addSpriteTile(s,(0x9<<2)|1,base+0x08,-8,-16);
	addSpriteTile(s,(0x9<<2)|1,base+0x0A,8,-16);
	addSpriteTile(s,(0x9<<2)|1,base+0x0C,-8,0);
	addSpriteTile(s,(0x9<<2)|1,base+0x0E,8,0);
}
//Boo
void drawSprite_071(sprite_t * s) {
	int base = findSpGfxFile(0x61);
	if(s->data[2]&1) {
		if(s->data[1]&2) {
			addSpriteTile(s,(0x9<<2)|1,base+0x26,0,0);
		} else {
			addSpriteTile(s,(0x9<<2)|1,base+0x00,-8,-8);
			addSpriteTile(s,(0x9<<2)|1,base+0x02,8,-8);
			addSpriteTile(s,(0x9<<2)|1,base+0x08,-8,8);
			addSpriteTile(s,(0x9<<2)|1,base+0x0A,8,8);
		}
	} else {
		addSpriteTile(s,(0x9<<2)|1,base+0x00,-8,-8);
		addSpriteTile(s,(0x9<<2)|1,base+0x02,8,-8);
		addSpriteTile(s,(0x9<<2)|1,base+0x08,-8,8);
		addSpriteTile(s,(0x9<<2)|1,base+0x0A,8,8);
		addSpriteTile(s,(0x9<<2)|1,base+0x26,24,0);
		addSpriteTile(s,(0x9<<2)|1,base+0x26,40,0);
		addSpriteTile(s,(0x9<<2)|1,base+0x26,56,0);
	}
}
//Train Bandit
void drawSprite_072(sprite_t * s) {
	int base = findSpGfxFile(0x2E);
	addSpriteTile(s,(0xC<<2)|1,base+0x08,0,0);
}
//Pump with large balloon platform
void drawSprite_073(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x4084,-8,-22);
	addSpriteTile(s,(0x9<<2),0x4085,8,-22);
	addSpriteTile(s,(0x9<<2),0x4094,-8,-6);
	addSpriteTile(s,(0x9<<2),0x4095,8,-6);
	addSpriteTile(s,(0x8<<2),0x003E,-8,8);
	addSpriteTile(s,(0x8<<2),0x003F,-0,8);
	addSpriteTile(s,(0x8<<2)|0x40,0x003F,8,8);
	addSpriteTile(s,(0x8<<2)|0x40,0x003E,16,8);
	addSpriteTile(s,(0x8<<2),0x003E,-8,-28);
	addSpriteTile(s,(0x8<<2),0x003F,-0,-28);
	addSpriteTile(s,(0x8<<2)|0x40,0x003F,8,-28);
	addSpriteTile(s,(0x8<<2)|0x40,0x003E,16,-28);
	int base = findSpGfxFile(0x65);
	addSpriteTile(s,(9<<2)|1,base+0x0E,14,6);
	BYTE tempBuf[44*2];
	for(int n=0; n<88; n++) {
		tempBuf[n] = romBuf[0x066E64+(n<<1)];
	}
	drawSpriteHDMAPolygon(s,tempBuf,0x8400,44,true,false,0x1C,0x0C);
}
//Spike
void drawSprite_074(sprite_t * s) {
	int base = findSpGfxFile(0x26);
	addSpriteTile(s,(0xA<<2)|1,base+0x00,0,0);
}
//Spike projectile
void drawSprite_075(sprite_t * s) {
	addSpriteTile(s,(0xA<<2),0x4060,0,0);
}
//Piro Dangle
void drawSprite_076(sprite_t * s) {
	int base = findSpGfxFile(0x45);
	addSpriteTile(s,(0x9<<2),0x4063,0,0);
	addSpriteTile(s,(0x9<<2),base+0x02,0,4);
	addSpriteTile(s,(0x9<<2)|0x40,base+0x02,8,4);
}
//Bullet Bill blaster, red
void drawSprite_078(sprite_t * s) {
	int base = findSpGfxFile(0x31);
	addSpriteTile(s,(0x9<<2)|1,base+0x02,0,-8);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,0,0);
}
//Bullet Bill blaster, yellow
void drawSprite_079(sprite_t * s) {
	int base = findSpGfxFile(0x31);
	addSpriteTile(s,(0xA<<2)|1,base+0x02,0,-8);
	addSpriteTile(s,(0xA<<2)|1,base+0x00,0,0);
}
//Bullet Bill blaster, green
void drawSprite_07A(sprite_t * s) {
	int base = findSpGfxFile(0x31);
	addSpriteTile(s,(0x8<<2)|1,base+0x02,0,-8);
	addSpriteTile(s,(0x8<<2)|1,base+0x00,0,0);
}
//Bullet Bill, red
void drawSprite_07B(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x4082,-8,-8);
	addSpriteTile(s,(0x9<<2),0x4083,8,-8);
	addSpriteTile(s,(0x9<<2),0x4092,-8,8);
	addSpriteTile(s,(0x9<<2),0x4093,8,8);
}
//Bullet Bill, yellow
void drawSprite_07C(sprite_t * s) {
	addSpriteTile(s,(0xA<<2),0x4082,-8,-8);
	addSpriteTile(s,(0xA<<2),0x4083,8,-8);
	addSpriteTile(s,(0xA<<2),0x4092,-8,8);
	addSpriteTile(s,(0xA<<2),0x4093,8,8);
}
//Bullet Bill, green
void drawSprite_07D(sprite_t * s) {
	int base = findSpGfxFile(0x31);
	addSpriteTile(s,(0x8<<2)|1,base+0x04,-8,-8);
	addSpriteTile(s,(0x8<<2)|1,base+0x06,8,-8);
	addSpriteTile(s,(0x8<<2)|1,base+0x08,-8,8);
	addSpriteTile(s,(0x8<<2)|1,base+0x0A,8,8);
}
//Squishy block dent
void drawSprite_07E(sprite_t * s) {
	addSpriteTile(s,(0x2<<2),0x40A3,-16,0);
	addSpriteTile(s,(0x2<<2),0x40A4,0,0);
	addSpriteTile(s,(0x2<<2),0x40A5,16,0);
}
//Log seesaw
void drawSprite_07F(sprite_t * s) {
	addSpriteTile(s,(0xC<<2),0x4046,18,-8);
	addSpriteTile(s,(0xC<<2),0x4047,34,-8);
	addSpriteTile(s,(0xC<<2),0x4056,18,8);
	addSpriteTile(s,(0xC<<2),0x4057,34,8);
	addSpriteTile(s,(0xC<<2),0x4046,-8,-8);
	addSpriteTile(s,(0xC<<2),0x4047,8,-8);
	addSpriteTile(s,(0xC<<2),0x4056,-8,8);
	addSpriteTile(s,(0xC<<2),0x4057,8,8);
	addSpriteTile(s,(0xC<<2),0x4046,-34,-8);
	addSpriteTile(s,(0xC<<2),0x4047,-18,-8);
	addSpriteTile(s,(0xC<<2),0x4056,-34,8);
	addSpriteTile(s,(0xC<<2),0x4057,-18,8);
}
//Lava Bubble
void drawSprite_080(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x408B,0,0);
}
//Chain Chomp
void drawSprite_082(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x00BD,4,8);
	addSpriteTile(s,(0x8<<2),0x00BD,-4,8);
	addSpriteTile(s,(0x8<<2),0x00BD,-12,8);
	addSpriteTile(s,(0x8<<2),0x00BD,-20,8);
	addSpriteTile(s,(0x8<<2),0x4099,-36,0);
}
//Ambient cloud for final boss
void drawSprite_083(sprite_t * s) {
	addSpriteTile(s,(0xF<<2)|1,0x012C,-16,0);
	addSpriteTile(s,(0xF<<2)|1,0x0142,0,8);
	addSpriteTile(s,(0xF<<2)|0xC1,0x0142,0,8);
	addSpriteTile(s,(0xF<<2)|0xC1,0x012C,16,0);
}
//Teleport screen exit
void drawSprite_084(sprite_t * s) {
	drawSpriteText(s,"Teleport\n  Exit  ");
}
//Harry Hedgehog
void drawSprite_085(sprite_t * s) {
	int base = findSpGfxFile(0x1E);
	addSpriteTile(s,(0x8<<2)|1,base+0x0A,0,0);
}
//Red Egg with 1-UP
void drawSprite_087(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|1,0x0082,0,0);
}
//Flatbed Ferry, red horizontal
void drawSprite_089(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|1,0x0028,-16,0);
	addSpriteTile(s,(0x9<<2)|1,0x0029,0,0);
	addSpriteTile(s,(0x9<<2)|1,0x002A,16,0);
}
//Flatbed Ferry, pink vertical
void drawSprite_08A(sprite_t * s) {
	addSpriteTile(s,(0xC<<2)|1,0x0028,-16,0);
	addSpriteTile(s,(0xC<<2)|1,0x0029,0,0);
	addSpriteTile(s,(0xC<<2)|1,0x002A,16,0);
}
//Mock Up
void drawSprite_08B(sprite_t * s) {
	int pal = (s->data[2]&1);
	pal = (pal+0x8)<<2;
	addSpriteTile(s,pal,0x44C4,-8,-8);
	addSpriteTile(s,pal,0x44C5,8,-8);
	addSpriteTile(s,pal,0x44D4,-8,8);
	addSpriteTile(s,pal,0x44D5,8,8);
}
//Yoshi at goal
void drawSprite_08C(sprite_t * s) {
	addSpriteTile(s,(0xF<<2),0x4168,-8,-8);
	addSpriteTile(s,(0xF<<2),0x4169,8,-8);
	addSpriteTile(s,(0xF<<2),0x4178,-8,8);
	addSpriteTile(s,(0xF<<2),0x4179,8,8);
}
//Fly Guy
void drawSprite_08D(sprite_t * s) {
	if(s->data[1]&2) {
		addSpriteTile(s,(0xB<<2)|1,0x00E3,0,17);
		addSpriteTile(s,(0x9<<2),0x0049,-2,22);
		addSpriteTile(s,(0x9<<2),0x006E,0,22);
		addSpriteTile(s,(0x9<<2),0x006F,8,22);	
	} else if(s->data[2]&1) {
		addSpriteTile(s,(0x9<<2)|1,0x00A0,0,16);
	} else {
		addSpriteTile(s,(0x8<<2)|1,0x00EA,0,16);
	}
	addSpriteTile(s,(0x9<<2),0x002C,0,-6);
	addSpriteTile(s,(0x9<<2)|0x40,0x002C,8,-6);
	addSpriteTile(s,(0x9<<2)|0x40,0x002F,6,14);
	addSpriteTile(s,(0x9<<2)|0x40,0x002F,1,14);
	addSpriteTile(s,(0x9<<2)|1,0x0088,0,0);
}
//Kamek in Bowser's Room
void drawSprite_08E(sprite_t * s) {
	int base = findSpGfxFile(0x67);
	addSpriteTile(s,(0x9<<2)|1,base+0x04,-1,3);
	addSpriteTile(s,(0x9<<2)|0x80,base+0x1F,-9,8);
	addSpriteTile(s,(0x9<<2)|1,base+0x02,-1,4);
	addSpriteTile(s,(0x9<<2)|1,base+0x04,8,3);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,0,-10);
}
//Grinder, swing
void drawSprite_08F(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x40C6,-78,-8);
	addSpriteTile(s,(0x8<<2),0x40C7,-62,-8);
	addSpriteTile(s,(0x8<<2),0x40D6,-78,8);
	addSpriteTile(s,(0x8<<2),0x40D7,-62,8);
	addSpriteTile(s,(0x8<<2),0x40C6,-58,-8);
	addSpriteTile(s,(0x8<<2),0x40C7,-42,-8);
	addSpriteTile(s,(0x8<<2),0x40D6,-58,8);
	addSpriteTile(s,(0x8<<2),0x40D7,-42,8);
	addSpriteTile(s,(0x8<<2),0x40C6,-38,-8);
	addSpriteTile(s,(0x8<<2),0x40C7,-22,-8);
	addSpriteTile(s,(0x8<<2),0x40D6,-38,8);
	addSpriteTile(s,(0x8<<2),0x40D7,-22,8);
	addSpriteTile(s,(0x8<<2),0x40C6,-18,-8);
	addSpriteTile(s,(0x8<<2),0x40C7,-2,-8);
	addSpriteTile(s,(0x8<<2),0x40D6,-18,8);
	addSpriteTile(s,(0x8<<2),0x40D7,-2,8);
}
//Dangling Ghost
void drawSprite_090(sprite_t * s) {
	BYTE tempBuf[18*2];
	memcpy(tempBuf,&romBuf[0x0352E3],36);
	tempBuf[9] += 0x10;
	tempBuf[11] += 0x20;
	tempBuf[13] += 0x30;
	tempBuf[15] += 0x40;
	tempBuf[17] += 0x40;
	tempBuf[19] += 0x40;
	tempBuf[21] += 0x40;
	tempBuf[23] += 0x40;
	tempBuf[25] += 0x30;
	tempBuf[27] += 0x20;
	tempBuf[29] += 0x10;
	BYTE tempBuf2[144*2];
	for(int n=0; n<18; n++) {
		int px0 = tempBuf[n<<1];
		int py0 = tempBuf[(n<<1)+1];
		int px1 = tempBuf[((n+1)%18)<<1];
		int py1 = tempBuf[(((n+1)%18)<<1)+1];
		int px2 = tempBuf[((n+2)%18)<<1];
		int py2 = tempBuf[(((n+2)%18)<<1)+1];
		if(px0&0x80) px0 -= 0x100;
		if(py0&0x80) py0 -= 0x100;
		if(px1&0x80) px1 -= 0x100;
		if(py1&0x80) py1 -= 0x100;
		if(px2&0x80) px2 -= 0x100;
		if(py2&0x80) py2 -= 0x100;
		for(int m=0; m<8; m++) {
			int offset = ((n<<3)+m)<<1;
			int c0 = (8-m)*(8-m);
			int c2 = m*m;
			int c1 = 128-c0-c2;
			tempBuf2[offset] = ((px0*c0)+(px1*c1)+(px2*c2))>>7;
			tempBuf2[offset+1] = ((py0*c0)+(py1*c1)+(py2*c2))>>7;
		}
	}
	drawSpriteHDMAPolygon(s,tempBuf2,0x8400,144,false,false,0,0);
	int base = findSpGfxFile(0x42);
	addSpriteTile(s,(0x8<<2),base+0x0A,-18,80);
	addSpriteTile(s,(0x8<<2),base+0x0A,-10,80);
	addSpriteTile(s,(0x8<<2)|1,base+0x24,-24,73);
	addSpriteTile(s,(0x8<<2)|1,base+0x26,-12,73);
}
//Group of 4 Toadies
void drawSprite_091(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|1,0x000E,-10,0);
	addSpriteTile(s,(0x9<<2),0x002C,-10,-8);
	addSpriteTile(s,(0x9<<2)|0x40,0x002C,-2,-8);
	addSpriteTile(s,(0x9<<2)|0x41,0x000E,10,0);
	addSpriteTile(s,(0x9<<2),0x002C,10,-8);
	addSpriteTile(s,(0x9<<2)|0x40,0x002C,18,-8);
	addSpriteTile(s,(0x9<<2)|1,0x000E,-6,8);
	addSpriteTile(s,(0x9<<2),0x002C,-6,0);
	addSpriteTile(s,(0x9<<2)|0x40,0x002C,2,0);
	addSpriteTile(s,(0x9<<2)|0x41,0x000E,6,8);
	addSpriteTile(s,(0x9<<2),0x002C,6,0);
	addSpriteTile(s,(0x9<<2)|0x40,0x002C,14,0);
}
//Melon Bug
void drawSprite_092(sprite_t * s) {
	int base = findSpGfxFile(0x1E);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,0,-8);
	addSpriteTile(s,(0x9<<2),base+0x04,0,8);
	addSpriteTile(s,(0x9<<2),base+0x05,8,8);
}
//Door
void drawSprite_093(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x440F,-8,-16);
	addSpriteTile(s,(0x8<<2)|0x40,0x440F,8,-16);
	addSpriteTile(s,(0x8<<2),0x441F,-8,0);
	addSpriteTile(s,(0x8<<2)|0x40,0x441F,8,0);
}
//Expansion Block
void drawSprite_094(sprite_t * s) {
	addSpriteTile(s,(0xC<<2),0x4074,0,0);
}
//Checkered platform
void drawSprite_095(sprite_t * s) {
	int spRef = (s->data[0]-0x95)<<4;
	spRef |= (0x8<<2)|1;
	addSpriteTile(s,spRef,0x409B,0,0);
}
//POW block
void drawSprite_097(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x40EA,-8,-16);
	addSpriteTile(s,(0x9<<2),0x40EB,8,-16);
	addSpriteTile(s,(0x9<<2),0x40FA,-8,0);
	addSpriteTile(s,(0x9<<2),0x40FB,8,0);
}
//Yoshi block
void drawSprite_098(sprite_t * s) {
	addSpriteTile(s,(0xD<<2),0x40EC,-8,-16);
	addSpriteTile(s,(0xD<<2),0x40ED,8,-16);
	addSpriteTile(s,(0xD<<2),0x40FC,-8,0);
	addSpriteTile(s,(0xD<<2),0x40FD,8,0);
}
//Spiny Egg
void drawSprite_099(sprite_t * s) {
	int base = findSpGfxFile(0x4F);
	addSpriteTile(s,(0xA<<2)|1,base+0x0E,0,0);
}
//Flatbed Ferry, green
void drawSprite_09A(sprite_t * s) {
	addSpriteTile(s,(0x8<<2)|1,0x00C0,0,0);
	addSpriteTile(s,(0x8<<2),0x00BD,-14,-9);
	addSpriteTile(s,(0x8<<2),0x00BD,-31,-22);
	addSpriteTile(s,(0x8<<2),0x00BD,-49,-35);
	addSpriteTile(s,(0x8<<2)|1,0x0028,-79,-48);
	addSpriteTile(s,(0x8<<2)|1,0x002A,-63,-48);
}
//Mace Guy
void drawSprite_09B(sprite_t * s) {
	int base = findSpGfxFile(0x59);
	addSpriteTile(s,(0x9<<2)|1,base+0x0C,0,0);
	addSpriteTile(s,(0x8<<2),0x00BD,0,2);
	addSpriteTile(s,(0x8<<2),0x00BD,-8,-3);
	addSpriteTile(s,(0x8<<2),0x00BD,-16,-8);
	addSpriteTile(s,(0xA<<2),0x4060,-32,-17);
}
//Mace Guy projectile
void drawSprite_09C(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x00BD,0,2);
	addSpriteTile(s,(0x8<<2),0x00BD,-8,-3);
	addSpriteTile(s,(0x8<<2),0x00BD,-16,-8);
	addSpriteTile(s,(0xA<<2),0x4060,-32,-17);
}
//!-switch
void drawSprite_09D(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x440C,-8,-16);
	addSpriteTile(s,(0x9<<2),0x440D,8,-16);
	addSpriteTile(s,(0x9<<2),0x441C,-8,0);
	addSpriteTile(s,(0x9<<2),0x441D,8,0);
}
//Chomp Rock
void drawSprite_09E(sprite_t * s) {
	addSpriteTile(s,(0xA<<2),0x4162,-8,-16);
	addSpriteTile(s,(0xA<<2),0x4163,8,-16);
	addSpriteTile(s,(0xA<<2),0x4172,-8,0);
	addSpriteTile(s,(0xA<<2),0x4173,8,0);
}
//Wild Ptooie Piranha
void drawSprite_09F(sprite_t * s) {
	int base = findSpGfxFile(0x29);
	addSpriteTile(s,(0x9<<2),base+0x1A,0,8);
	addSpriteTile(s,(0x9<<2)|0x40,base+0x1A,8,8);
	addSpriteTile(s,(0x9<<2),base+0x0A,4,0);
	addSpriteTile(s,(0x8<<2),0x446C,-23,-20);
	addSpriteTile(s,(0x8<<2),0x446D,-7,-20);
	addSpriteTile(s,(0x8<<2),0x447C,-23,-4);
	addSpriteTile(s,(0x8<<2),0x447D,-7,-4);
}
//Tulip
void drawSprite_0A0(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x4506,-8,-8);
	addSpriteTile(s,(0x9<<2),0x4516,-8,8);
	addSpriteTile(s,(0x9<<2)|0x40,0x4506,8,-8);
	addSpriteTile(s,(0x9<<2)|0x40,0x4516,8,8);
	addSpriteTile(s,(0x9<<2),0x4503,0,-8);
	addSpriteTile(s,(0x9<<2),0x4513,0,8);
}
//Pot for Potted Spiked Fun Guy
void drawSprite_0A1(sprite_t * s) {
	int base = findSpGfxFile(0x36);
	addSpriteTile(s,(0xC<<2)|1,base+0x0E,0,0);
}
//Thunder Lakitu projectile
void drawSprite_0A2(sprite_t * s) {
	int base = findSpGfxFile(0x4B);
	addSpriteTile(s,(0xA<<2)|0x40,base+0x19,0,0);
	addSpriteTile(s,(0xA<<2),base+0x19,8,0);
	addSpriteTile(s,(0xA<<2)|0xC0,base+0x19,0,8);
	addSpriteTile(s,(0xA<<2)|0x80,base+0x19,8,8);
}
//Nep-Enut/Gargantua Blargg
void drawSprite_0A5(sprite_t * s) {
	int base = findSpGfxFile(0x53);
	addSpriteTile(s,(0x8<<2)|0x41,base+0x00,-11,-100);
	addSpriteTile(s,(0x8<<2)|0x41,base+0x00,26,-113);
	drawSpriteHDMAPolygon(s,&romBuf[0x0146E4],0x8400,62,false,false,8,-8);
	addSpriteTile(s,(0x8<<2)|0x41,base+0x0C,10,-142);
	addSpriteTile(s,(0x8<<2)|0x81,base+0x0C,10,-126);
	addSpriteTile(s,(0x8<<2)|0x41,base+0x0C,-10,-142);
	addSpriteTile(s,(0x8<<2)|0x81,base+0x0C,-10,-126);
	addSpriteTile(s,(0x8<<2)|0x41,base+0x08,-8,-152);
	addSpriteTile(s,(0x8<<2)|1,base+0x08,8,-152);
	addSpriteTile(s,(0x8<<2)|0x41,base+0x0A,-17,-162);
	addSpriteTile(s,(0x8<<2)|1,base+0x0A,16,-161);
	addSpriteTile(s,(0x8<<2)|0x40,base+0x07,14,-139);
	addSpriteTile(s,(0x8<<2)|0x40,base+0x07,-6,-139);
}
//Incoming Chomp, in background
void drawSprite_0A6(sprite_t * s) {
	int base = findSpGfxFile(0x1B);
	addSpriteTile(s,(0xE<<2)|1,base+0x05,0,0);
}
//Incoming Chomp, flock in background
void drawSprite_0A7(sprite_t * s) {
	int base = findSpGfxFile(0x1B);
	addSpriteTile(s,(0xE<<2)|1,base+0x05,-8,-8);
	addSpriteTile(s,(0xE<<2)|1,base+0x05,8,-8);
	addSpriteTile(s,(0xE<<2)|1,base+0x05,-16,0);
	addSpriteTile(s,(0xE<<2)|1,base+0x05,0,0);
	addSpriteTile(s,(0xE<<2)|1,base+0x05,16,0);
}
//Falling Incoming Chomp
void drawSprite_0A8(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x40C2,-8,-8);
	addSpriteTile(s,(0x9<<2)|0x40,0x40C2,8,-8);
	addSpriteTile(s,(0x9<<2),0x40D2,-8,8);
	addSpriteTile(s,(0x9<<2)|0x40,0x40D2,8,8);
}
//Incoming Chomp shadow
void drawSprite_0A9(sprite_t * s) {
	int base = findSpGfxFile(0x1B);
	addSpriteTile(s,(0xD<<2),base+0x00,-24,1);
	addSpriteTile(s,(0xD<<2),base+0x02,-8,1);
	addSpriteTile(s,(0xD<<2)|0x40,base+0x02,8,1);
	addSpriteTile(s,(0xD<<2)|0x40,base+0x00,24,1);
}
//Shy Guy in background
void drawSprite_0AA(sprite_t * s) {
	int base = findSpGfxFile(0x1B);
	addSpriteTile(s,(0xE<<2),base+0x04,4,0);
}
//Fill Eggs
void drawSprite_0AB(sprite_t * s) {
	addSpriteTile(s,(0x8<<2)|1,0x0082,0,0);
}
//Signal arrow from final boss
void drawSprite_0AC(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x00C4,11,-32);
	addSpriteTile(s,(0x9<<2)|0x40,0x00C4,13,-32);
	addSpriteTile(s,(0x9<<2)|0xC0,0x00C5,8,-24);
	addSpriteTile(s,(0x9<<2)|0x80,0x00C5,16,-24);
}
//Message Block
void drawSprite_0AD(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x4161,0,0);
}
//Hookbill Koopa
void drawSprite_0AE(sprite_t * s) {
	addSpriteTile(s,(0xF<<2)|1,0x010E,25,11);
	addSpriteTile(s,(0xF<<2)|1,0x010C,17,19);
	addSpriteTile(s,(0xF<<2),0x011E,33,27);
	addSpriteTile(s,(0xF<<2),0x0120,17,11);
	addSpriteTile(s,(0xF<<2),0x0120,-18,18);
	addSpriteTile(s,(0xF<<2)|1,0x0120,-18,18);
	addSpriteTile(s,(0xF<<2),0x012C,-26,26);
	addSpriteTile(s,(0xF<<2)|1,0x0122,-18,10);
	addSpriteTile(s,(0xE<<2),0x44EC,-45,-7);
	addSpriteTile(s,(0xE<<2),0x44ED,-29,-7);
	addSpriteTile(s,(0xE<<2),0x44FC,-45,9);
	addSpriteTile(s,(0xE<<2),0x44FD,-29,9);
	addSpriteTile(s,(0xE<<2),0x44EA,32,1);
	addSpriteTile(s,(0xE<<2),0x0124,-8,11);
	addSpriteTile(s,(0xE<<2)|1,0x0124,-8,11);
	addSpriteTile(s,(0xE<<2)|1,0x0126,0,19);
	addSpriteTile(s,(0xE<<2),0x0135,-8,27);
	addSpriteTile(s,(0xE<<2),0x44E8,-1,6);
	addSpriteTile(s,(0xE<<2),0x44E9,15,6);
	addSpriteTile(s,(0xE<<2),0x44F8,-1,22);
	addSpriteTile(s,(0xE<<2),0x44F9,15,22);
}
//Morph Bubble, Car
void drawSprite_0AF(sprite_t * s) {
	addSpriteTile(s,(0xD<<2),0x4566,0,0);
	addSpriteTile(s,(0x8<<2)|1,0x009C,-7,-7);
	addSpriteTile(s,(0x8<<2)|1,0x007E,7,-7);
	addSpriteTile(s,(0x8<<2)|0xC1,0x007E,-7,7);
	addSpriteTile(s,(0x8<<2)|0x81,0x007E,7,7);
}
//Morph Bubble, Mole Tank
void drawSprite_0B0(sprite_t * s) {
	addSpriteTile(s,(0xD<<2),0x4567,0,0);
	addSpriteTile(s,(0x8<<2)|1,0x009C,-7,-7);
	addSpriteTile(s,(0x8<<2)|1,0x007E,7,-7);
	addSpriteTile(s,(0x8<<2)|0xC1,0x007E,-7,7);
	addSpriteTile(s,(0x8<<2)|0x81,0x007E,7,7);
}
//Morph Bubble, Helicopter
void drawSprite_0B1(sprite_t * s) {
	addSpriteTile(s,(0xD<<2),0x4576,0,0);
	addSpriteTile(s,(0x8<<2)|1,0x009C,-7,-7);
	addSpriteTile(s,(0x8<<2)|1,0x007E,7,-7);
	addSpriteTile(s,(0x8<<2)|0xC1,0x007E,-7,7);
	addSpriteTile(s,(0x8<<2)|0x81,0x007E,7,7);
}
//Morph Bubble, Train
void drawSprite_0B2(sprite_t * s) {
	addSpriteTile(s,(0xD<<2),0x4577,0,0);
	addSpriteTile(s,(0x8<<2)|1,0x009C,-7,-7);
	addSpriteTile(s,(0x8<<2)|1,0x007E,7,-7);
	addSpriteTile(s,(0x8<<2)|0xC1,0x007E,-7,7);
	addSpriteTile(s,(0x8<<2)|0x81,0x007E,7,7);
}
//Fuzzy cloud
void drawSprite_0B3(sprite_t * s) {
	addSpriteTile(s,(0xB<<2)|1,0x00F0,0,0);
}
//Morph Bubble, Submarine
void drawSprite_0B4(sprite_t * s) {
	addSpriteTile(s,(0xD<<2),0x417F,0,0);
	addSpriteTile(s,(0x8<<2)|1,0x009C,-7,-7);
	addSpriteTile(s,(0x8<<2)|1,0x007E,7,-7);
	addSpriteTile(s,(0x8<<2)|0xC1,0x007E,-7,7);
	addSpriteTile(s,(0x8<<2)|0x81,0x007E,7,7);
}
//Baron Von Zeppelin, Giant Egg
void drawSprite_0CD(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|1,0x00A8,0,-32);
	addSpriteTile(s,(0x9<<2),0x00BC,4,-18);
	addSpriteTile(s,(0x9<<2),0x00BC,4,-14);
	addSpriteTile(s,(0x9<<2)|1,0x0182,4,-8);
	addSpriteTile(s,(0x9<<2)|1,0x0180,-4,0);
	addSpriteTile(s,(0x9<<2),0x0192,12,8);
	addSpriteTile(s,(0x9<<2),0x0184,-4,-8);
}
//Baby Bowser projectile
void drawSprite_0CE(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x408C,-8,-24);
	addSpriteTile(s,(0x9<<2)|0x40,0x408C,8,-24);
	addSpriteTile(s,(0x9<<2)|0x80,0x408C,-8,-8);
	addSpriteTile(s,(0x9<<2)|0xC0,0x408C,8,-8);
}
//Baby Bowser quake
void drawSprite_0CF(sprite_t * s) {
	drawSpriteText(s,"Baby Bowser\n   Quake   ");
}
//Horizontal pipe exit right
void drawSprite_0D0(sprite_t * s) {
	drawSpriteText(s,"Horizontal  Pipe\n   Exit Right   ");
}
//Hidden vertical pipe entrance
void drawSprite_0D1(sprite_t * s) {
	drawSpriteText(s,"  Vertical  Pipe  \nExit Down (Hidden)");
}
//Marching Milde
void drawSprite_0D2(sprite_t * s) {
	for(int j=0; j<64; j++) {
		int row = romBuf[0x050A51+j];
		addSpriteTile(s,0,0x8100|row,-0x80,j-0x78);
		row = romBuf[0x050A90+j];
		addSpriteTile(s,0,0x8100|row,-0x80,j-0x38);
	}
	addSpriteTile(s,(0xF<<2),0x4264,0,-110);
	addSpriteTile(s,(0xF<<2),0x4265,16,-110);
	addSpriteTile(s,(0xF<<2),0x4274,0,-94);
	addSpriteTile(s,(0xF<<2),0x4275,16,-94);
	addSpriteTile(s,(0xF<<2),0x4268,-52,-112);
	addSpriteTile(s,(0xF<<2),0x4269,-36,-112);
	addSpriteTile(s,(0xF<<2),0x4278,-52,-96);
	addSpriteTile(s,(0xF<<2),0x4279,-36,-96);
	addSpriteTile(s,(0xE<<2)|0x40,0x4268,-11,-112);
	addSpriteTile(s,(0xE<<2)|0x40,0x4269,-27,-112);
	addSpriteTile(s,(0xE<<2)|0x40,0x4278,-11,-96);
	addSpriteTile(s,(0xE<<2)|0x40,0x4279,-27,-96);
	addSpriteTile(s,(0xF<<2)|0xC0,0x4266,13,2);
	addSpriteTile(s,(0xF<<2)|0xC0,0x4267,-3,2);
	addSpriteTile(s,(0xF<<2)|0xC0,0x4276,13,-14);
	addSpriteTile(s,(0xF<<2)|0xC0,0x4277,-3,-14);
	addSpriteTile(s,(0xE<<2),0x4266,-20,-13);
	addSpriteTile(s,(0xE<<2),0x4267,-4,-13);
	addSpriteTile(s,(0xE<<2),0x4276,-20,3);
	addSpriteTile(s,(0xE<<2),0x4277,-4,3);
}
//Marching Milde, 1/2
void drawSprite_0D3(sprite_t * s) {
	int base = findSpGfxFile(0x6F);
	addSpriteTile(s,(0xF<<2)|0x41,base+0x0D,10,20);
	addSpriteTile(s,(0xF<<2)|0x41,base+0x0E,-6,20);
	addSpriteTile(s,(0xE<<2),0x426A,-24,-26);
	addSpriteTile(s,(0xE<<2),0x426B,-8,-26);
	addSpriteTile(s,(0xE<<2),0x427A,-24,-10);
	addSpriteTile(s,(0xE<<2),0x427B,-8,-10);
	addSpriteTile(s,(0xE<<2),0x426C,-24,6);
	addSpriteTile(s,(0xE<<2),0x426D,-8,6);
	addSpriteTile(s,(0xE<<2),0x427C,-24,22);
	addSpriteTile(s,(0xE<<2),0x427D,-8,22);
	addSpriteTile(s,(0xE<<2)|0x40,0x426A,24,-26);
	addSpriteTile(s,(0xE<<2)|0x40,0x426B,8,-26);
	addSpriteTile(s,(0xE<<2)|0x40,0x427A,24,-10);
	addSpriteTile(s,(0xE<<2)|0x40,0x427B,8,-10);
	addSpriteTile(s,(0xE<<2)|0x40,0x426C,24,6);
	addSpriteTile(s,(0xE<<2)|0x40,0x426D,8,6);
	addSpriteTile(s,(0xE<<2)|0x40,0x427C,24,22);
	addSpriteTile(s,(0xE<<2)|0x40,0x427D,8,22);
	addSpriteTile(s,(0xF<<2)|1,base+0x00,-17,-19);
	addSpriteTile(s,(0xF<<2)|1,base+0x04,6,-16);
	addSpriteTile(s,(0xF<<2)|1,base+0x02,-5,-19);
	addSpriteTile(s,(0xE<<2)|1,base+0x0D,-10,20);
	addSpriteTile(s,(0xE<<2)|1,base+0x0E,-2,20);
}
//Marching Milde, 1/4
void drawSprite_0D4(sprite_t * s) {
	int base = findSpGfxFile(0x6D);
	addSpriteTile(s,(0xC<<2)|1,base+0x08,-8,-8);
	addSpriteTile(s,(0xC<<2)|1,base+0x0A,8,-8);
	addSpriteTile(s,(0xC<<2)|1,base+0x2A,-8,8);
	addSpriteTile(s,(0xC<<2)|1,base+0x2C,8,8);
}
//Ambient mountains from Hookbill boss
void drawSprite_0D5(sprite_t * s) {
	int base = findSpGfxFile(0x1A);
	addSpriteTile(s,(0xF<<2)|1,base+0x0E,232,40);
	addSpriteTile(s,(0xF<<2)|1,base+0x08,200,72);
	addSpriteTile(s,(0xF<<2)|1,base+0x08,184,16);
	addSpriteTile(s,(0xF<<2)|1,base+0x0C,144,104);
	addSpriteTile(s,(0xF<<2)|1,base+0x06,144,88);
	addSpriteTile(s,(0xF<<2)|1,base+0x02,144,72);
	addSpriteTile(s,(0xF<<2)|1,base+0x0E,136,48);
	addSpriteTile(s,(0xF<<2)|1,base+0x0A,128,104);
	addSpriteTile(s,(0xF<<2)|1,base+0x04,128,88);
	addSpriteTile(s,(0xF<<2)|1,base+0x00,128,72);
	addSpriteTile(s,(0xF<<2)|1,base+0x0E,96,24);
	addSpriteTile(s,(0xF<<2)|1,base+0x08,88,80);
	addSpriteTile(s,(0xF<<2)|1,base+0x0E,48,48);
	addSpriteTile(s,(0xF<<2)|1,base+0x0E,40,8);
	addSpriteTile(s,(0xF<<2)|1,base+0x08,32,40);
	addSpriteTile(s,(0xF<<2)|1,base+0x0C,16,56);
	addSpriteTile(s,(0xF<<2)|1,base+0x06,16,40);
	addSpriteTile(s,(0xF<<2)|1,base+0x02,16,24);
	addSpriteTile(s,(0xF<<2)|1,base+0x0E,8,88);
	addSpriteTile(s,(0xF<<2)|1,base+0x0A,0,56);
	addSpriteTile(s,(0xF<<2)|1,base+0x04,0,40);
	addSpriteTile(s,(0xF<<2)|1,base+0x00,0,24);
}
//Blarggwich
void drawSprite_0D6(sprite_t * s) {
	drawSpriteHDMAPolygon(s,&romBuf[0x0368FC],0x8400,16,false,false,0,0);
	addSpriteTile(s,(0x8<<2)|1,0x0028,-24,-137);
	addSpriteTile(s,(0x8<<2)|1,0x0029,-8,-137);
	addSpriteTile(s,(0x8<<2)|1,0x002A,8,-137);
	addSpriteTile(s,(0x8<<2)|1,0x010C,-18,-127);
	addSpriteTile(s,(0x8<<2)|0xC1,0x010C,-18,-111);
	addSpriteTile(s,(0x8<<2)|1,0x010C,2,-127);
	addSpriteTile(s,(0x8<<2)|0xC1,0x010C,2,-111);
	addSpriteTile(s,(0x8<<2)|1,0x0108,0,-137);
	addSpriteTile(s,(0x8<<2)|0x41,0x0108,-16,-137);
	addSpriteTile(s,(0x8<<2)|1,0x010A,9,-147);
	addSpriteTile(s,(0x8<<2)|0x41,0x010A,-24,-146);
	addSpriteTile(s,(0x8<<2),0x0107,-14,-124);
	addSpriteTile(s,(0x8<<2),0x0107,6,-124);
}
//Sluggy the Unshaven
void drawSprite_0D7(sprite_t * s) {
	BYTE tempBuf[128*2];
	for(int n=0; n<16; n++) {
		int px0 = romBuf[0x015109+(n<<1)];
		int py0 = romBuf[0x01510A+(n<<1)];
		int px1 = romBuf[0x015109+(((n+1)&0xF)<<1)];
		int py1 = romBuf[0x01510A+(((n+1)&0xF)<<1)];
		int px2 = romBuf[0x015109+(((n+2)&0xF)<<1)];
		int py2 = romBuf[0x01510A+(((n+2)&0xF)<<1)];
		if(px0&0x80) px0 -= 0x100;
		if(py0&0x80) py0 -= 0x100;
		if(px1&0x80) px1 -= 0x100;
		if(py1&0x80) py1 -= 0x100;
		if(px2&0x80) px2 -= 0x100;
		if(py2&0x80) py2 -= 0x100;
		if(py0<-72) py0 = -72;
		if(py1<-72) py1 = -72;
		if(py2<-72) py2 = -72;
		for(int m=0; m<8; m++) {
			int offset = ((n<<3)+m)<<1;
			int c0 = (8-m)*(8-m);
			int c2 = m*m;
			int c1 = 128-c0-c2;
			tempBuf[offset] = ((px0*c0)+(px1*c1)+(px2*c2))>>7;
			tempBuf[offset+1] = ((py0*c0)+(py1*c1)+(py2*c2))>>7;
		}
	}
	drawSpriteHDMAPolygon(s,tempBuf,0x8000,128,false,false,0x08,0x48);
	addSpriteTile(s,(0x8<<2),0x4678,-61,11);
	addSpriteTile(s,(0x8<<2)|0x80,0x4678,-61,27);
	addSpriteTile(s,(0x8<<2),0x4678,-27,11);
	addSpriteTile(s,(0x8<<2)|0x80,0x4678,-27,27);
	addSpriteTile(s,(0x8<<2),0x4679,-41,55);
	addSpriteTile(s,(0xF<<2),0x466A,18,17);
	addSpriteTile(s,(0xF<<2),0x466B,34,17);
	addSpriteTile(s,(0xF<<2),0x467A,18,33);
	addSpriteTile(s,(0xF<<2),0x467B,34,33);
	addSpriteTile(s,(0xF<<2)|0xC0,0x466A,34,65);
	addSpriteTile(s,(0xF<<2)|0xC0,0x466B,18,65);
	addSpriteTile(s,(0xF<<2)|0xC0,0x467A,34,49);
	addSpriteTile(s,(0xF<<2)|0xC0,0x467B,18,49);
	addSpriteTile(s,(0xF<<2),0x4669,19,69);
	addSpriteTile(s,(0xF<<2)|0xC0,0x4669,19,85);
	addSpriteTile(s,(0xF<<2),0x4668,44,87);
	addSpriteTile(s,(0x8<<2),0x467F,-104,-1);
	addSpriteTile(s,(0x8<<2)|0x40,0x466F,-88,-17);
	addSpriteTile(s,(0x8<<2)|0x40,0x466E,-72,-17);
	addSpriteTile(s,(0x8<<2)|0x40,0x467F,-88,-1);
	addSpriteTile(s,(0x8<<2)|0x40,0x467E,-72,-1);
	addSpriteTile(s,(0x8<<2)|0x40,0x467F,59,3);
	addSpriteTile(s,(0x8<<2),0x466E,29,-13);
	addSpriteTile(s,(0x8<<2),0x466F,45,-13);
	addSpriteTile(s,(0x8<<2),0x467E,29,3);
	addSpriteTile(s,(0x8<<2),0x467F,45,3);
	addSpriteTile(s,(0x8<<2)|0x40,0x467F,49,21);
	addSpriteTile(s,(0xF<<2),0x466C,-16,54);
	addSpriteTile(s,(0xF<<2),0x466D,0,54);
	addSpriteTile(s,(0xF<<2),0x467C,-16,70);
	addSpriteTile(s,(0xF<<2),0x467D,0,70);
}
//Chomp signboard
void drawSprite_0D8(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x448C,-8,-8);
	addSpriteTile(s,(0x9<<2),0x448D,8,-8);
	addSpriteTile(s,(0x9<<2),0x449C,-8,8);
	addSpriteTile(s,(0x9<<2),0x449D,8,8);
}
//Fishin' Lakitu
void drawSprite_0D9(sprite_t * s) {
	int base = findSpGfxFile(0x49);
	addSpriteTile(s,(0x8<<2),base+0x1D,9,-3);
	addSpriteTile(s,(0xA<<2)|0x41,base+0x08,4,4);
	addSpriteTile(s,(0xA<<2)|1,base+0x08,-4,4);
	addSpriteTile(s,(0x9<<2),base+0x0D,3,-13);
	addSpriteTile(s,(0x9<<2)|1,base+0x04,-3,-6);
	addSpriteTile(s,(0x9<<2)|1,base+0x24,-10,-5);
	addSpriteTile(s,(0x9<<2)|1,base+0x26,-18,11);
}
//Flower pot
void drawSprite_0DA(sprite_t * s) {
	int base = findSpGfxFile(0x44);
	addSpriteTile(s,(0x9<<2)|1,base+0x02,-8,-22);
	addSpriteTile(s,(0x9<<2)|1,base+0x04,8,-22);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,0,-38);
	addSpriteTile(s,(0x9<<2)|1,base+0x08,-4,0);
	addSpriteTile(s,(0x9<<2)|1,base+0x09,4,0);
	addSpriteTile(s,(0x9<<2),base+0x0D,-4,-8);
	addSpriteTile(s,(0x9<<2),base+0x0E,4,-8);
	addSpriteTile(s,(0x9<<2),base+0x0F,12,-8);
}
//Squishy block
void drawSprite_0DB(sprite_t * s) {
	BYTE tempBuf[32] = {0xE0,0xEC,0xE1,0xEA,0xE2,0xE9,0xE4,0xE8,
		0x1C,0xE8,0x1E,0xE9,0x1F,0xEA,0x20,0xEC,
		0x20,0x14,0x1F,0x16,0x1E,0x17,0x1C,0x18,
		0xE4,0x18,0xE2,0x17,0xE1,0x16,0xE0,0x14};
	drawSpriteHDMAPolygon(s,tempBuf,0x8400,16,false,false,0,8);
}
//Snowball
void drawSprite_0DC(sprite_t * s) {
	addSpriteTile(s,(0xB<<2),0x40E8,-8,-8);
	addSpriteTile(s,(0xB<<2),0x40E9,8,-8);
	addSpriteTile(s,(0xB<<2),0x40F8,-8,8);
	addSpriteTile(s,(0xB<<2),0x40F9,8,8);
}
//Boss closer, Naval Piranha
void drawSprite_0DD(sprite_t * s) {
	drawSpriteText(s," Boss  Closer \nNaval  Piranha");
}
//Falling rock from background
void drawSprite_0DE(sprite_t * s) {
	int base = findSpGfxFile(0x20);
	addSpriteTile(s,(0xE<<2)|1,base+0x00,-16,0);
	addSpriteTile(s,(0xE<<2)|1,base+0x02,0,0);
	addSpriteTile(s,(0xE<<2)|1,base+0x04,16,0);
}
//Piscatory Pete
void drawSprite_0DF(sprite_t * s) {
	int base = findSpGfxFile(0x3E);
	int pal = ((s->data[2]&1)^0xF)<<2;
	addSpriteTile(s,pal|1,base+0x08,0,0);
}
//Preying Mantas
void drawSprite_0E0(sprite_t * s) {
	int base = findSpGfxFile(0x3E);
	addSpriteTile(s,(0xE<<2)|1,base+0x00,0,0);
}
//Loch Nestor
void drawSprite_0E1(sprite_t * s) {
	int base = findSpGfxFile(0x3E);
	addSpriteTile(s,(0xF<<2)|1,base+0x0E,0,0);
}
//Boo Blah
void drawSprite_0E2(sprite_t * s) {
	int base = findSpGfxFile(0x22);
	if(s->data[2]&1) {
		addSpriteTile(s,(0x9<<2)|0x81,base+0x0C,-10,32);
		addSpriteTile(s,(0x9<<2)|0x81,base+0x0C,0,32);
		addSpriteTile(s,(0x9<<2)|0x81,base+0x09,-8,16);
		addSpriteTile(s,(0x9<<2)|0x81,base+0x0A,0,16);
		addSpriteTile(s,(0x9<<2)|0x81,base+0x28,-8,0);
		addSpriteTile(s,(0x9<<2)|0x81,base+0x29,0,0);
	} else {
		addSpriteTile(s,(0x9<<2)|1,base+0x0C,-10,-32);
		addSpriteTile(s,(0x9<<2)|1,base+0x0C,0,-32);
		addSpriteTile(s,(0x9<<2)|1,base+0x09,-8,-16);
		addSpriteTile(s,(0x9<<2)|1,base+0x0A,0,-16);
		addSpriteTile(s,(0x9<<2)|1,base+0x28,-8,0);
		addSpriteTile(s,(0x9<<2)|1,base+0x29,0,0);
	}
}
//Boo Blah with Piro Dangle
void drawSprite_0E3(sprite_t * s) {
	int base = findSpGfxFile(0x22);
	if(s->data[2]&1) {
		addSpriteTile(s,(0x9<<2)|0x81,base+0x0C,-10,32);
		addSpriteTile(s,(0x9<<2)|0x81,base+0x0C,0,32);
		addSpriteTile(s,(0x9<<2)|0x81,base+0x09,-8,16);
		addSpriteTile(s,(0x9<<2)|0x81,base+0x0A,0,16);
		addSpriteTile(s,(0x9<<2)|0x81,base+0x28,-8,0);
		addSpriteTile(s,(0x9<<2)|0x81,base+0x29,0,0);
		base = findSpGfxFile(0x45);
		addSpriteTile(s,(0x9<<2),0x4063,-4,43);
		addSpriteTile(s,(0x9<<2),base+0x02,-4,47);
		addSpriteTile(s,(0x9<<2)|0x40,base+0x02,4,47);
	} else {
		addSpriteTile(s,(0x9<<2)|1,base+0x0C,-10,-32);
		addSpriteTile(s,(0x9<<2)|1,base+0x0C,0,-32);
		addSpriteTile(s,(0x9<<2)|1,base+0x09,-8,-16);
		addSpriteTile(s,(0x9<<2)|1,base+0x0A,0,-16);
		addSpriteTile(s,(0x9<<2)|1,base+0x28,-8,0);
		addSpriteTile(s,(0x9<<2)|1,base+0x29,0,0);
		base = findSpGfxFile(0x45);
		addSpriteTile(s,(0x9<<2),0x4063,-4,-43);
		addSpriteTile(s,(0x9<<2),base+0x02,-4,-39);
		addSpriteTile(s,(0x9<<2)|0x40,base+0x02,4,-39);
	}
}
//Pokey
void drawSprite_0E4(sprite_t * s) {
	int base = findSpGfxFile(0x24);
	addSpriteTile(s,(0x8<<2)|1,base+0x0A,0,-16);
	addSpriteTile(s,(0x8<<2)|1,base+0x0C,0,0);
	addSpriteTile(s,(0x8<<2)|1,0x008A,0,-32);
}
//Needlenose, green
void drawSprite_0E5(sprite_t * s) {
	addSpriteTile(s,(0x8<<2)|1,0x008A,0,0);
}
//Gusty
void drawSprite_0E6(sprite_t * s) {
	int offs = (s->data[1]&2)|(s->data[2]&1);
	int base = findSpGfxFile(0x25);
	if(offs==1 || offs==2) {
		addSpriteTile(s,(0xA<<2)|0x41,base+0x06,8,-1);
		addSpriteTile(s,(0xA<<2)|0x40,base+0x09,-8,1);
		addSpriteTile(s,(0xA<<2)|0x40,base+0x19,-8,9);
		addSpriteTile(s,(0xA<<2)|0x40,base+0x08,0,0);
		addSpriteTile(s,(0xA<<2)|0x40,base+0x18,0,8);
	} else {
		addSpriteTile(s,(0xA<<2)|1,base+0x06,-8,-1);
		addSpriteTile(s,(0xA<<2),base+0x09,16,1);
		addSpriteTile(s,(0xA<<2),base+0x19,16,9);
		addSpriteTile(s,(0xA<<2),base+0x08,8,0);
		addSpriteTile(s,(0xA<<2),base+0x18,8,8);
	}
	if(offs&2) {
		addSpriteTile(s,(0xC<<2),0x0077,16,0);
	}
}
//Burt
void drawSprite_0E7(sprite_t * s) {
	if(s->data[2]&1) {
		addSpriteTile(s,(0x8<<2),0x448E,-8,-16);
		addSpriteTile(s,(0x8<<2),0x448F,8,-16);
		addSpriteTile(s,(0x8<<2),0x449E,-8,0);
		addSpriteTile(s,(0x8<<2),0x449F,8,0);
		addSpriteTile(s,(0x8<<2)|0x40,0x009E,0,8);
		addSpriteTile(s,(0x8<<2),0x009E,8,8);
	} else {
		addSpriteTile(s,(0x8<<2),0x448E,-8,-16);
		addSpriteTile(s,(0x8<<2),0x448F,8,-16);
		addSpriteTile(s,(0x8<<2),0x449E,-8,0);
		addSpriteTile(s,(0x8<<2),0x449F,8,0);
		addSpriteTile(s,(0x8<<2)|0x40,0x009E,0,8);
		addSpriteTile(s,(0x8<<2),0x009E,8,8);
		addSpriteTile(s,(0x8<<2),0x448E,24,-16);
		addSpriteTile(s,(0x8<<2),0x448F,40,-16);
		addSpriteTile(s,(0x8<<2),0x449E,24,0);
		addSpriteTile(s,(0x8<<2),0x449F,40,0);
		addSpriteTile(s,(0x8<<2)|0x40,0x009E,32,8);
		addSpriteTile(s,(0x8<<2),0x009E,40,8);
	}
}
//Goonie
void drawSprite_0E8(sprite_t * s) {
	int base = findSpGfxFile(0x2A);
	if(s->data[1]&2) {
		if(s->data[2]&1) {
			addSpriteTile(s,(0xA<<2)|0x41,base+0x04,-8,0);
			addSpriteTile(s,(0xA<<2)|1,base+0x22,16,0);
			addSpriteTile(s,(0xA<<2),base+0x24,32,0);
			addSpriteTile(s,(0xA<<2),base+0x34,8,4);
			addSpriteTile(s,(0xA<<2)|0x40,base+0x24,-31,0);
			addSpriteTile(s,(0xA<<2)|0x41,base+0x00,8,0);
			addSpriteTile(s,(0xA<<2)|0x40,base+0x34,-7,4);
			addSpriteTile(s,(0xA<<2)|0x41,base+0x22,-23,0);
			addSpriteTile(s,(0xC<<2),0x0077,8,0);
		} else {
			addSpriteTile(s,(0xA<<2)|1,base+0x04,8,0);
			addSpriteTile(s,(0xA<<2)|0x41,base+0x22,-16,0);
			addSpriteTile(s,(0xA<<2)|0x40,base+0x24,-24,0);
			addSpriteTile(s,(0xA<<2)|0x40,base+0x34,0,4);
			addSpriteTile(s,(0xA<<2),base+0x24,39,0);
			addSpriteTile(s,(0xA<<2)|1,base+0x00,-8,0);
			addSpriteTile(s,(0xA<<2),base+0x34,15,4);
			addSpriteTile(s,(0xA<<2)|1,base+0x22,23,0);
			addSpriteTile(s,(0xC<<2),0x0077,8,0);
		}
	} else {
		addSpriteTile(s,(0xA<<2)|1,base+0x04,8,0);
		addSpriteTile(s,(0xA<<2)|0x41,base+0x22,-16,0);
		addSpriteTile(s,(0xA<<2)|0x40,base+0x24,-24,0);
		addSpriteTile(s,(0xA<<2)|0x40,base+0x34,0,4);
		addSpriteTile(s,(0xA<<2),base+0x24,39,0);
		addSpriteTile(s,(0xA<<2)|1,base+0x00,-8,0);
		addSpriteTile(s,(0xA<<2),base+0x34,15,4);
		addSpriteTile(s,(0xA<<2)|1,base+0x22,23,0);
	}
}
//Group of 3 Flightless Goonies
void drawSprite_0E9(sprite_t * s) {
	int base = findSpGfxFile(0x2A);
	addSpriteTile(s,(0xA<<2)|1,base+0x00,-8,-2);
	addSpriteTile(s,(0xA<<2),base+0x04,8,-2);
	addSpriteTile(s,(0xA<<2)|1,base+0x08,3,6);
	addSpriteTile(s,(0xA<<2)|1,base+0x00,8,-2);
	addSpriteTile(s,(0xA<<2),base+0x04,24,-2);
	addSpriteTile(s,(0xA<<2)|1,base+0x08,19,6);
	addSpriteTile(s,(0xA<<2)|1,base+0x00,24,-2);
	addSpriteTile(s,(0xA<<2),base+0x04,40,-2);
	addSpriteTile(s,(0xA<<2)|1,base+0x08,35,6);
}
//Cloud Drop, vertical
void drawSprite_0EA(sprite_t * s) {
	int base = findSpGfxFile(0x2C);
	addSpriteTile(s,(0xA<<2)|1,base+0x00,0,0);
	addSpriteTile(s,(0xA<<2),base+0x0C,4,16);
	addSpriteTile(s,(0xA<<2),base+0x1C,3,1);
}
//Cloud Drop, horizontal
void drawSprite_0EB(sprite_t * s) {
	int base = findSpGfxFile(0x2D);
	addSpriteTile(s,(0xA<<2)|1,base+0x00,-4,0);
	addSpriteTile(s,(0xA<<2),base+0x1B,12,5);
}
//Flamer Guy
void drawSprite_0EC(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x4144,-8,-20);
	addSpriteTile(s,(0x9<<2),0x4145,8,-20);
	addSpriteTile(s,(0x9<<2),0x4154,-8,-4);
	addSpriteTile(s,(0x9<<2),0x4155,8,-4);
	addSpriteTile(s,(0x9<<2)|1,0x01AE,-1,-5);
	addSpriteTile(s,(0x8<<2),0x009E,0,8);
	addSpriteTile(s,(0x8<<2),0x009E,8,8);
}
//Eggo-Dil
void drawSprite_0EE(sprite_t * s) {
	int base = findSpGfxFile(0x2F);
	addSpriteTile(s,(0x8<<2)|1,base+0x00,-8,0);
	addSpriteTile(s,(0x8<<2)|0x41,base+0x00,8,0);
	addSpriteTile(s,(0xA<<2)|1,base+0x08,0,-34);
	addSpriteTile(s,(0xA<<2)|1,base+0x08,-15,-23);
	addSpriteTile(s,(0xA<<2)|1,base+0x08,15,-23);
	addSpriteTile(s,(0xA<<2)|1,base+0x08,-10,-5);
	addSpriteTile(s,(0xA<<2)|1,base+0x08,9,-5);
	addSpriteTile(s,(0xC<<2),0x4106,-8,-30);
	addSpriteTile(s,(0xC<<2),0x4107,8,-30);
	addSpriteTile(s,(0xC<<2),0x4116,-8,-14);
	addSpriteTile(s,(0xC<<2),0x4117,8,-14);
}
//Eggo-Dil face
void drawSprite_0EF(sprite_t * s) {
	addSpriteTile(s,(0xC<<2),0x4106,-8,-8);
	addSpriteTile(s,(0xC<<2),0x4107,8,-8);
	addSpriteTile(s,(0xC<<2),0x4116,-8,8);
	addSpriteTile(s,(0xC<<2),0x4117,8,8);
}
//Eggo-Dil petal
void drawSprite_0F0(sprite_t * s) {
	int base = findSpGfxFile(0x2F);
	addSpriteTile(s,(0xA<<2)|1,base+0x08,0,0);
}
//Bubble-Plant
void drawSprite_0F1(sprite_t * s) {
	int base = findSpGfxFile(0x1F);
	addSpriteTile(s,(0x9<<2),base+0x1A,0,8);
	addSpriteTile(s,(0x9<<2),base+0x0A,8,8);
	addSpriteTile(s,(0xC<<2)|1,base+0x06,0,-7);
}
//Stilt Guy
void drawSprite_0F2(sprite_t * s) {
	int offset = ((s->data[2]&1)|(s->data[1]&2))<<1;
	int pal = ((romBuf[0x0209B8+offset]-1)<<1)|0x20;
	int base = findSpGfxFile(0x37);
	addSpriteTile(s,pal|1,base+0x0A,1,-8);
	addSpriteTile(s,pal,base+0x17,9,8);
	addSpriteTile(s,pal,base+0x06,0,-21);
	addSpriteTile(s,pal|1,base+0x00,3,-32);
	addSpriteTile(s,pal|1,base+0x0A,1,-23);
	addSpriteTile(s,pal|1,base+0x0A,1,-38);
	addSpriteTile(s,pal,base+0x06,12,-21);
	addSpriteTile(s,pal,base+0x07,-3,8);
}
//Woozy Guy
void drawSprite_0F3(sprite_t * s) {
	int offset = ((s->data[2]&1)|(s->data[1]&2))<<1;
	int pal = ((romBuf[0x0209B8+offset]-1)<<1)|0x20;
	addSpriteTile(s,pal,0x4072,0,0);
}
//Slugger
void drawSprite_0F5(sprite_t * s) {
	addSpriteTile(s,(0xA<<2)|1,0x0140,-2,-10);
	addSpriteTile(s,(0xA<<2),0x0154,-1,4);
	addSpriteTile(s,(0xA<<2),0x0155,7,4);
	addSpriteTile(s,(0xC<<2)|1,0x0148,6,-8);
	addSpriteTile(s,(0x8<<2),0x009E,4,8);
}
//Huffin' Puffin group
void drawSprite_0F6(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x4120,-8,-19);
	addSpriteTile(s,(0x9<<2),0x4121,8,-19);
	addSpriteTile(s,(0x9<<2),0x4130,-8,-3);
	addSpriteTile(s,(0x9<<2),0x4131,8,-3);
	addSpriteTile(s,(0x9<<2),0x009E,0,8);
	addSpriteTile(s,(0x9<<2)|1,0x0102,24,-4);
	addSpriteTile(s,(0x9<<2),0x009E,28,8);
	addSpriteTile(s,(0x9<<2)|1,0x0102,40,-4);
	addSpriteTile(s,(0x9<<2),0x009E,44,8);
	addSpriteTile(s,(0x9<<2)|1,0x0102,56,-4);
	addSpriteTile(s,(0x9<<2),0x009E,60,8);
}
//Barney Bubble
void drawSprite_0F7(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x009E,1,9);
	addSpriteTile(s,(0xC<<2)|1,0x014E,0,0);
	addSpriteTile(s,(0x8<<2),0x009E,10,9);
}
//Blow Hard
void drawSprite_0F8(sprite_t * s) {
	int base = findSpGfxFile(0x36);
	addSpriteTile(s,(8<<2)|1,base+0x00,-8,0);
	addSpriteTile(s,(8<<2)|1,base+0x02,8,0);
	addSpriteTile(s,(8<<2),0x4122,-16,-20);
	addSpriteTile(s,(8<<2),0x4123,0,-20);
	addSpriteTile(s,(8<<2),0x4132,-16,-4);
	addSpriteTile(s,(8<<2),0x4133,0,-4);
}
//Needlenose, yellow
void drawSprite_0F9(sprite_t * s) {
	addSpriteTile(s,(0xA<<2)|1,0x008A,0,0);
}
//Flower (SuperFX texture)
void drawSprite_0FA(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x4042,-8,-8);
	addSpriteTile(s,(0x9<<2),0x4043,8,-8);
	addSpriteTile(s,(0x9<<2),0x4052,-8,8);
	addSpriteTile(s,(0x9<<2),0x4053,8,8);
}
//Spear Guy, long spear
void drawSprite_0FB(sprite_t * s) {
	int base = findSpGfxFile(0x35);
	addSpriteTile(s,(0x9<<2),base+0x05,2,10);
	addSpriteTile(s,(0x9<<2),base+0x05,5,12);
	addSpriteTile(s,(0x9<<2),base+0x17,-4,-5);
	addSpriteTile(s,(0x9<<2)|0x80,base+0x17,-4,3);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,0,-4);
	addSpriteTile(s,(0x9<<2),base+0x04,10,-12);
	addSpriteTile(s,(0x9<<2),base+0x07,5,-12);
	addSpriteTile(s,(0x9<<2),base+0x16,7,-20);
	addSpriteTile(s,(0x9<<2),base+0x06,5,-28);
}
//Spear Guy, short spear
void drawSprite_0FC(sprite_t * s) {
	int base = findSpGfxFile(0x35);
	addSpriteTile(s,(0x9<<2),base+0x05,2,10);
	addSpriteTile(s,(0x9<<2),base+0x05,5,12);
	addSpriteTile(s,(0x9<<2),base+0x17,-4,-5);
	addSpriteTile(s,(0x9<<2)|0x80,base+0x17,-4,3);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,0,-4);
	addSpriteTile(s,(0x9<<2),base+0x04,10,-12);
	addSpriteTile(s,(0x9<<2),base+0x16,7,-8);
	addSpriteTile(s,(0x9<<2),base+0x06,5,-16);
}
//Zeus Guy
void drawSprite_0FD(sprite_t * s) {
	addSpriteTile(s,(0xA<<2)|1,0x0124,2,2);
	addSpriteTile(s,(0x9<<2),0x0136,0,12);
	addSpriteTile(s,(0x9<<2),0x0136,11,12);
	addSpriteTile(s,(0x8<<2)|1,0x0120,0,-7);
	addSpriteTile(s,(0xA<<2),0x0126,-5,3);
	addSpriteTile(s,(0xA<<2),0x0126,5,4);
}
//Zeus Guy projectile
void drawSprite_0FE(sprite_t * s) {
	addSpriteTile(s,(0x9<<2)|1,0x0042,-8,-4);
	addSpriteTile(s,(0x9<<2)|0x41,0x0042,8,-4);
	addSpriteTile(s,(0x9<<2)|0x81,0x0042,-8,4);
	addSpriteTile(s,(0x9<<2)|0xC1,0x0042,8,4);
}
//Poochy
void drawSprite_0FF(sprite_t * s) {
	int base = findSpGfxFile(0x3A);
	addSpriteTile(s,(0x9<<2)|1,base+0x00,-16,-7);
	addSpriteTile(s,(0x9<<2)|1,base+0x02,0,-7);
	addSpriteTile(s,(0x9<<2)|1,base+0x0A,5,4);
	addSpriteTile(s,(0x9<<2),base+0x0C,-3,12);
	addSpriteTile(s,(0x9<<2)|1,base+0x04,-6,2);
	addSpriteTile(s,(0x9<<2)|0x40,base+0x3C,15,-5);
	addSpriteTile(s,(0x9<<2),base+0x3F,-10,-6);
}
//Bubble with 1-UP
void drawSprite_100(sprite_t * s) {
	addSpriteTile(s,(0x9<<2),0x0049,-1,4);
	addSpriteTile(s,(0x9<<2),0x006E,1,4);
	addSpriteTile(s,(0x9<<2),0x006F,8,4);
	addSpriteTile(s,(0x8<<2)|1,0x009C,-7,-7);
	addSpriteTile(s,(0x8<<2)|1,0x007E,7,-7);
	addSpriteTile(s,(0x8<<2)|0xC1,0x007E,-7,7);
	addSpriteTile(s,(0x8<<2)|0x81,0x007E,7,7);
}
//Rotating spike mace, 1 end
void drawSprite_101(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x410C,-8,-31);
	addSpriteTile(s,(0x8<<2),0x410D,8,-31);
	addSpriteTile(s,(0x8<<2),0x411C,-8,-15);
	addSpriteTile(s,(0x8<<2),0x411D,8,-15);
	addSpriteTile(s,(0x8<<2),0x410C,-8,-59);
	addSpriteTile(s,(0x8<<2),0x410D,8,-59);
	addSpriteTile(s,(0x8<<2),0x411C,-8,-43);
	addSpriteTile(s,(0x8<<2),0x411D,8,-43);
	addSpriteTile(s,(0x8<<2),0x410A,-8,-87);
	addSpriteTile(s,(0x8<<2),0x410B,8,-87);
	addSpriteTile(s,(0x8<<2),0x411A,-8,-71);
	addSpriteTile(s,(0x8<<2),0x411B,8,-71);
}
//Rotating spike mace, 2 end
void drawSprite_102(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x410C,-8,-31);
	addSpriteTile(s,(0x8<<2),0x410D,8,-31);
	addSpriteTile(s,(0x8<<2),0x411C,-8,-15);
	addSpriteTile(s,(0x8<<2),0x411D,8,-15);
	addSpriteTile(s,(0x8<<2),0x410C,-8,-59);
	addSpriteTile(s,(0x8<<2),0x410D,8,-59);
	addSpriteTile(s,(0x8<<2),0x411C,-8,-43);
	addSpriteTile(s,(0x8<<2),0x411D,8,-43);
	addSpriteTile(s,(0x8<<2),0x410A,-8,-87);
	addSpriteTile(s,(0x8<<2),0x410B,8,-87);
	addSpriteTile(s,(0x8<<2),0x411A,-8,-71);
	addSpriteTile(s,(0x8<<2),0x411B,8,-71);
	addSpriteTile(s,(0x8<<2)|0xC0,0x410C,8,31);
	addSpriteTile(s,(0x8<<2)|0xC0,0x410D,-8,31);
	addSpriteTile(s,(0x8<<2)|0xC0,0x411C,8,15);
	addSpriteTile(s,(0x8<<2)|0xC0,0x411D,-8,15);
	addSpriteTile(s,(0x8<<2)|0xC0,0x410C,8,59);
	addSpriteTile(s,(0x8<<2)|0xC0,0x410D,-8,59);
	addSpriteTile(s,(0x8<<2)|0xC0,0x411C,8,43);
	addSpriteTile(s,(0x8<<2)|0xC0,0x411D,-8,43);
	addSpriteTile(s,(0x8<<2)|0xC0,0x410A,8,87);
	addSpriteTile(s,(0x8<<2)|0xC0,0x410B,-8,87);
	addSpriteTile(s,(0x8<<2)|0xC0,0x411A,8,71);
	addSpriteTile(s,(0x8<<2)|0xC0,0x411B,-8,71);
}
//Boo Guys controlling rotating spike mace
void drawSprite_103(sprite_t * s) {
	int base = findSpGfxFile(0x3C);
	addSpriteTile(s,(0x8<<2)|1,base+0x00,12,0);
	addSpriteTile(s,(0x8<<2)|1,base+0x08,-4,0);
}
//Jean de Fillet
void drawSprite_104(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x454E,-8,-8);
	addSpriteTile(s,(0x8<<2),0x454F,8,-8);
	addSpriteTile(s,(0x8<<2),0x455E,-8,8);
	addSpriteTile(s,(0x8<<2),0x455F,8,8);
}
//Boo Guys carring bomb
void drawSprite_105(sprite_t * s) {
	addSpriteTile(s,(0x8<<2)|1,0x0160,0,-8);
}
//Watermelon projectile
void drawSprite_107(sprite_t * s) {
	addSpriteTile(s,(0x8<<2),0x00BC,0,0);
}
//Milde
void drawSprite_108(sprite_t * s) {
	int base = findSpGfxFile(0x29);
	addSpriteTile(s,(0xC<<2)|1,base+0x04,0,0);
}
//Tap-Tap
void drawSprite_109(sprite_t * s) {
	addSpriteTile(s,(0x8<<2)|1,0x01AB,0,0);
	addSpriteTile(s,(0x8<<2),0x009E,0,8);
	addSpriteTile(s,(0x8<<2),0x009E,8,8);
}
//Falling spike mace
void drawSprite_10C(sprite_t * s) {
	int base = findSpGfxFile(0x3F);
	
	
	
}
//Boo Guys controlling falling spike mace
void drawSprite_10D(sprite_t * s) {
	int base = findSpGfxFile(0x3F);
	
	
	
}
//Boo Man Bluff
void drawSprite_10F(sprite_t * s) {
	int base = findSpGfxFile(0x30);
	addSpriteTile(s,(0x8<<2),base+0x06,-4,12);
	addSpriteTile(s,(0x8<<2),base+0x07,4,12);
	addSpriteTile(s,(0x8<<2),base+0x08,12,12);
	addSpriteTile(s,(0x8<<2)|1,base+0x00,-4,-4);
	addSpriteTile(s,(0x8<<2)|1,base+0x01,4,-4);
	addSpriteTile(s,(0x8<<2),base+0x0B,0,-2);
	addSpriteTile(s,(0x8<<2),base+0x09,3,10);
}
//Flower (character)
void drawSprite_110(sprite_t * s) {
	int base = findSpGfxFile(0x71);
	addSpriteTile(s,(0x9<<2)|1,base+0x04,-8,-8);
	addSpriteTile(s,(0x9<<2)|1,base+0x06,8,-8);
	addSpriteTile(s,(0x9<<2)|0xC1,base+0x06,-8,8);
	addSpriteTile(s,(0x9<<2)|0xC1,base+0x04,8,8);
	addSpriteTile(s,(0x9<<2)|1,base+0x02,0,0);
}
//Georgette Jelly
void drawSprite_111(sprite_t * s) {
	int base = findSpGfxFile(0x41);
	
	
	
}
//Georgette Jelly, flattened
void drawSprite_112(sprite_t * s) {
	int base = findSpGfxFile(0x41);
	
	
	
}
//Snifit
void drawSprite_113(sprite_t * s) {
	int base = findSpGfxFile(0x40);
	
	
	
}
//Snifit projectile
void drawSprite_114(sprite_t * s) {
	int base = findSpGfxFile(0x40);
	
	
	
}
//Coin
void drawSprite_115(sprite_t * s) {
	addSpriteTile(s,(0xA<<2)|1,0x00A0,0,0);
}
//TODO
//Caged Ghost squeezed in tunnel
void drawSprite_193(sprite_t * s) {
	drawSpriteHDMAPolygon(s,&romBuf[0x035A61],0x8400,27,true,false,0,-0x30);
	
	
	
}
//TODO
//Graphics/palette changer command
void drawSprite_1BA(sprite_t * s) {
	char spStr[256];
	int spRef = (s->data[0]-0xBA);
	snprintf(spStr,256,"Graphics/Palette\n   Changer %02X   ",spRef);
	drawSpriteText(s,spStr);
}
//Very slow auto-scroll command
void drawSprite_1CA(sprite_t * s) {
	drawSpriteText(s," Very Slow \nAuto-Scroll");
}
//Special auto-scroll command
void drawSprite_1CB(sprite_t * s) {
	char spStr[256];
	int spRef = (s->data[0]-0xCB);
	snprintf(spStr,256,"   Special   \nAuto-Scroll %d",spRef);
	drawSpriteText(s,spStr);
}
//Slow auto-scroll command
void drawSprite_1D4(sprite_t * s) {
	drawSpriteText(s,"   Slow    \nAuto-Scroll");
}
//TODO
//Lock horizontal scroll
void drawSprite_1D6(sprite_t * s) {
	drawSpriteText(s,"Lock  Scroll\n Horizontal ");
}
//TODO

//Sprite function pointer table and updater
void drawSprite_unused(sprite_t * s) {}
void (*spriteDrawFunc[0x200])(sprite_t * s) = {
	//000
	drawSprite_000,drawSprite_001,drawSprite_002,drawSprite_003,
	drawSprite_004,drawSprite_005,drawSprite_006,drawSprite_007,
	drawSprite_008,drawSprite_009,drawSprite_00A,drawSprite_00B,
	drawSprite_00C,drawSprite_00D,drawSprite_00E,drawSprite_00F,
	//010
	drawSprite_010,drawSprite_011,drawSprite_012,drawSprite_013,
	drawSprite_014,drawSprite_015,drawSprite_016,drawSprite_017,
	drawSprite_018,drawSprite_019,drawSprite_01A,drawSprite_01B,
	drawSprite_01C,drawSprite_01D,drawSprite_01E,drawSprite_01F,
	//020
	drawSprite_020,drawSprite_021,drawSprite_022,drawSprite_022,
	drawSprite_022,drawSprite_022,drawSprite_026,drawSprite_027,
	drawSprite_028,drawSprite_029,drawSprite_02A,drawSprite_02A,
	drawSprite_02C,drawSprite_02D,drawSprite_02E,drawSprite_02F,
	//030
	drawSprite_030,drawSprite_031,drawSprite_030,drawSprite_030,
	drawSprite_034,drawSprite_034,drawSprite_036,drawSprite_037,
	drawSprite_038,drawSprite_039,drawSprite_03A,drawSprite_03B,
	drawSprite_03C,drawSprite_03D,drawSprite_03E,drawSprite_03F,
	//040
	drawSprite_040,drawSprite_041,drawSprite_042,drawSprite_043,
	drawSprite_043,drawSprite_045,drawSprite_046,drawSprite_047,
	drawSprite_048,drawSprite_049,drawSprite_049,drawSprite_049,
	drawSprite_04C,drawSprite_04D,drawSprite_04E,drawSprite_04F,
	//050
	drawSprite_050,drawSprite_051,drawSprite_052,drawSprite_048,
	drawSprite_054,drawSprite_055,drawSprite_056,drawSprite_057,
	drawSprite_058,drawSprite_059,drawSprite_05A,drawSprite_05B,
	drawSprite_058,drawSprite_unused,drawSprite_05E,drawSprite_05E,
	//060
	drawSprite_060,drawSprite_061,drawSprite_062,drawSprite_063,
	drawSprite_064,drawSprite_065,drawSprite_066,drawSprite_067,
	drawSprite_068,drawSprite_069,drawSprite_06A,drawSprite_06B,
	drawSprite_06C,drawSprite_06D,drawSprite_06D,drawSprite_06F,
	//070
	drawSprite_070,drawSprite_071,drawSprite_072,drawSprite_073,
	drawSprite_074,drawSprite_075,drawSprite_076,drawSprite_076,
	drawSprite_078,drawSprite_079,drawSprite_07A,drawSprite_07B,
	drawSprite_07C,drawSprite_07D,drawSprite_07E,drawSprite_07F,
	//080
	drawSprite_080,drawSprite_080,drawSprite_082,drawSprite_083,
	drawSprite_084,drawSprite_085,drawSprite_unused,drawSprite_087,
	drawSprite_059,drawSprite_089,drawSprite_08A,drawSprite_08B,
	drawSprite_08C,drawSprite_08D,drawSprite_08E,drawSprite_08F,
	//090
	drawSprite_090,drawSprite_091,drawSprite_092,drawSprite_093,
	drawSprite_094,drawSprite_095,drawSprite_095,drawSprite_097,
	drawSprite_098,drawSprite_099,drawSprite_09A,drawSprite_09B,
	drawSprite_09C,drawSprite_09D,drawSprite_09E,drawSprite_09F,
	//0A0
	drawSprite_0A0,drawSprite_0A1,drawSprite_0A2,drawSprite_020,
	drawSprite_020,drawSprite_0A5,drawSprite_0A6,drawSprite_0A7,
	drawSprite_0A8,drawSprite_0A9,drawSprite_0AA,drawSprite_0AB,
	drawSprite_0AC,drawSprite_0AD,drawSprite_0AE,drawSprite_0AF,
	//0B0
	drawSprite_0B0,drawSprite_0B1,drawSprite_0B2,drawSprite_0B3,
	drawSprite_0B4,drawSprite_067,drawSprite_067,drawSprite_067,
	drawSprite_067,drawSprite_067,drawSprite_067,drawSprite_067,
	drawSprite_067,drawSprite_067,drawSprite_067,drawSprite_067,
	//0C0
	drawSprite_067,drawSprite_067,drawSprite_067,drawSprite_067,
	drawSprite_067,drawSprite_067,drawSprite_067,drawSprite_067,
	drawSprite_067,drawSprite_067,drawSprite_012,drawSprite_067,
	drawSprite_067,drawSprite_0CD,drawSprite_0CE,drawSprite_0CF,
	//0D0
	drawSprite_0D0,drawSprite_0D1,drawSprite_0D2,drawSprite_0D3,
	drawSprite_0D4,drawSprite_0D5,drawSprite_0D6,drawSprite_0D7,
	drawSprite_0D8,drawSprite_0D9,drawSprite_0DA,drawSprite_0DB,
	drawSprite_0DC,drawSprite_0DD,drawSprite_0DE,drawSprite_0DF,
	//0E0
	drawSprite_0E0,drawSprite_0E1,drawSprite_0E2,drawSprite_0E3,
	drawSprite_0E4,drawSprite_0E5,drawSprite_0E6,drawSprite_0E7,
	drawSprite_0E8,drawSprite_0E9,drawSprite_0EA,drawSprite_0EB,
	drawSprite_0EC,drawSprite_0EC,drawSprite_0EE,drawSprite_0EF,
	//0F0
	drawSprite_0F0,drawSprite_0F1,drawSprite_0F2,drawSprite_0F3,
	drawSprite_0F1,drawSprite_0F5,drawSprite_0F6,drawSprite_0F7,
	drawSprite_0F8,drawSprite_0F9,drawSprite_0FA,drawSprite_0FB,
	drawSprite_0FC,drawSprite_0FD,drawSprite_0FE,drawSprite_0FF,
	//100
	drawSprite_100,drawSprite_101,drawSprite_102,drawSprite_103,
	drawSprite_104,drawSprite_105,drawSprite_105,drawSprite_107,
	drawSprite_108,drawSprite_109,drawSprite_109,drawSprite_109,
	drawSprite_10C,drawSprite_10D,drawSprite_003,drawSprite_10F,
	//110
	drawSprite_110,drawSprite_111,drawSprite_112,drawSprite_113,
	drawSprite_114,drawSprite_115,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_099,drawSprite_unused,drawSprite_unused,
	//120
	drawSprite_unused,drawSprite_unused,drawSprite_021,drawSprite_021,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	//130
	drawSprite_unused,drawSprite_04E,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	//140
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_06C,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	//150
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	//160
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	//170
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	//180
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	//190
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_193,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	//1A0
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_115,
	//1B0
	drawSprite_unused,drawSprite_unused,drawSprite_115,drawSprite_020,
	drawSprite_unused,drawSprite_020,drawSprite_unused,drawSprite_020,
	drawSprite_unused,drawSprite_020,drawSprite_1BA,drawSprite_1BA,
	drawSprite_1BA,drawSprite_1BA,drawSprite_1BA,drawSprite_1BA,
	//1C0
	drawSprite_1BA,drawSprite_1BA,drawSprite_1BA,drawSprite_1BA,
	drawSprite_1BA,drawSprite_1BA,drawSprite_1BA,drawSprite_1BA,
	drawSprite_1BA,drawSprite_1BA,drawSprite_1CA,drawSprite_1CB,
	drawSprite_1CB,drawSprite_1CB,drawSprite_1CB,drawSprite_1CB,
	//1D0
	drawSprite_1CB,drawSprite_1CB,drawSprite_1CB,drawSprite_1CB,
	drawSprite_1D4,drawSprite_unused,drawSprite_1D6,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	//1E0
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	//1F0
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused,
	drawSprite_unused,drawSprite_unused,drawSprite_unused,drawSprite_unused};
int setSpriteContext(int ctx) {
	int prevCtx = curSpCtx;
	curSpCtx = ctx;
	return prevCtx;
}
void drawSprites() {
	for(int n = 0; n < spriteContexts[curSpCtx].sprites.size(); n++) {
		sprite_t * thisSprite = &spriteContexts[curSpCtx].sprites[n];
		int id = thisSprite->data[0]|(thisSprite->data[1]<<8);
		id &= 0x1FF;
		spriteDrawFunc[id](thisSprite);
	}
}
void drawSingleSprite(int n) {
	sprite_t * thisSprite = &spriteContexts[curSpCtx].sprites[n];
	int id = thisSprite->data[0]|(thisSprite->data[1]<<8);
	id &= 0x1FF;
	spriteDrawFunc[id](thisSprite);
}
void dispSprites(DWORD * pixelBuf,int width,int height,RECT rect) {
	for(int n = 0; n < spriteContexts[curSpCtx].sprites.size(); n++) {
		sprite_t * thisSprite = &spriteContexts[curSpCtx].sprites[n];
		int spX = thisSprite->data[2]<<4;
		int spY = (thisSprite->data[1]&0xFE)<<3;
		for(int i=0; i<thisSprite->tiles.size(); i++) {
			sprite_tile_t * thisSpriteTile = &thisSprite->tiles[i];
			BYTE props = thisSpriteTile->props;
			WORD tile = thisSpriteTile->tile;
			int sptX = spX+thisSpriteTile->offsX;
			int sptY = spY+thisSpriteTile->offsY;
			switch(tile&0xC000) {
				case 0x0000: {
					if(tile&0x2000) dispMap8Tile(pixelBuf,width,height,props,tile,{sptX,sptY});
					else dispMap8Tile(pixelBuf,width,height,props,tile+0x480,{sptX,sptY});
					break;
				}
				case 0x4000: {
					dispSuperFXTexture(pixelBuf,width,height,props,tile&0x3FFF,{sptX,sptY});
					break;
				}
				case 0x8000: {
					dispBackgroundRow(pixelBuf,width,height,tile&0x7FF,{sptX,sptY});
					break;
				}
				case 0xC000: {
					dispMap8Char(pixelBuf,width,height,0xFF,0xFFFFFF,tile&0x7F,{sptX,sptY});
					break;
				}
			}
		}
	}
}
void initOtherSpriteBuffers() {
	int spTs = ((levelHeader[4]&7)<<4)|(levelHeader[5]>>4);
	for(int i=0; i<6; i++) {
		spGfxFiles[i] = romBuf[0x003039+i+(spTs*6)];
	}
}

/////////////////////
//SPRITE MANAGEMENT//
/////////////////////
//Predicate for deletion
bool sprite_delPred(sprite_t & un) {
	return un.selected;
}

//Load/save
void loadSprites(BYTE * data) {
	//Clear buffers
	for(int i=0; i<0x8000; i++) {
		spriteContexts[curSpCtx].assocSprites[i].clear();
		spriteContexts[curSpCtx].invalidSprites[i] = false;
	}
	spriteContexts[curSpCtx].sprites.clear();
	//Reload buffer with sprite data
	while(true) {
		//Create entry
		sprite_t entry;
		//Get data bytes and check for end
		entry.data[0] = *data++;
		entry.data[1] = *data++;
		if(entry.data[0]==0xFF && entry.data[1]==0xFF) break;
		entry.data[2] = *data++;
		entry.dataSize = 3;
		//Init other elements to sane values
		entry.selected = false;
		entry.tiles.clear();
		//Push back
		spriteContexts[curSpCtx].sprites.push_back(entry);
	}
}
int saveSprites(BYTE * data) {
	//Init stuff
	int curSz = 0;
	//Save sprite data
	for(int n=0; n<spriteContexts[curSpCtx].sprites.size(); n++) {
		//Shorten the names of these for convenience
		BYTE * thisData = spriteContexts[curSpCtx].sprites[n].data;
		int thisDataSz = spriteContexts[curSpCtx].sprites[n].dataSize;
		//Copy data
		memcpy(&data[curSz],thisData,thisDataSz);
		//Increment stuff
		curSz += thisDataSz;
	}
}

//Manipulation (internal)
void addToSpriteSelection(int index) {
	//Select sprite
	sprite_t thisSprite = spriteContexts[0].sprites[index];
	thisSprite.selected = true;
	//Mark occupied tiles as invalid
	for(int i=0; i<thisSprite.tiles.size(); i++) {
		sprite_tile_t thisTile = thisSprite.tiles[i];
		for(int j=0; j<thisTile.numOccupiedTiles; j++) {
			spriteContexts[0].invalidSprites[thisTile.occupiedTiles[j]] = true;
		}
	}
}
void removeFromSpriteSelection(int index) {
	//Deselect sprite
	sprite_t thisSprite = spriteContexts[0].sprites[index];
	thisSprite.selected = false;
	//Mark occupied tiles as invalid
	for(int i=0; i<thisSprite.tiles.size(); i++) {
		sprite_tile_t thisTile = thisSprite.tiles[i];
		for(int j=0; j<thisTile.numOccupiedTiles; j++) {
			spriteContexts[0].invalidSprites[thisTile.occupiedTiles[j]] = true;
		}
	}
}
void clearSpriteSelection() {
	//Deselect all sprites
	for(int n=0; n<spriteContexts[0].sprites.size(); n++) {
		sprite_t thisSprite = spriteContexts[0].sprites[n];
		thisSprite.selected = false;
		//Mark occupied tiles as invalid
		for(int i=0; i<thisSprite.tiles.size(); i++) {
			sprite_tile_t thisTile = thisSprite.tiles[i];
			for(int j=0; j<thisTile.numOccupiedTiles; j++) {
				spriteContexts[0].invalidSprites[thisTile.occupiedTiles[j]] = true;
			}
		}
	}
}

//Manipulation
int selectSprites(RECT rect,bool ctrl) {
	if(ctrl) {
		//TODO
		for(int n=0; n<spriteContexts[0].sprites.size(); n++) {
			//TODO
		}
		//TODO
	} else {
		//TODO
		for(int n=0; n<spriteContexts[0].sprites.size(); n++) {
			//TODO
		}
		//TODO
	}
}
void insertSprites(int x,int y) {
	int numSelectedSprites = 0;
	int minX = 0x8000,minY = 0x8000;
	int maxX = 0,maxY = 0;
	//Check if any sprites are to be pasted
	for(int n=0; n<spriteContexts[0].sprites.size(); n++) {
		sprite_t thisSprite = spriteContexts[0].sprites[n];
		if(thisSprite.selected) {
			int xpos = thisSprite.data[2];
			int ypos = thisSprite.data[1]>>1;
			if(xpos<minX) minX = xpos;
			if(ypos<minY) minY = ypos;
			if(xpos>maxX) maxX = xpos;
			if(ypos>maxY) maxY = ypos;
			numSelectedSprites++;
		}
	}
	if(numSelectedSprites) {
		//Determine if any sprites will be out of bounds after this operation,
		//and if so, terminate
		//TODO
		//Paste selected sprites
		//TODO
	} else {
		//Determine if any sprites will be out of bounds after this operation,
		//and if so, terminate
		//TODO
		//Insert current sprite in selection dialog
		//TODO
	}
}
void deleteSprites() {
	//Invalidate occupied tiles
	for(int n=0; n<spriteContexts[0].sprites.size(); n++) {
		sprite_t thisSprite = spriteContexts[0].sprites[n];
		if(thisSprite.selected) {
			//Mark occupied tiles as invalid
			for(int i=0; i<thisSprite.tiles.size(); i++) {
				sprite_tile_t thisTile = thisSprite.tiles[i];
				for(int j=0; j<thisTile.numOccupiedTiles; j++) {
					spriteContexts[0].invalidSprites[thisTile.occupiedTiles[j]] = true;
				}
			}
		}
	}
	//Delete selected sprites
	remove_if(spriteContexts[0].sprites.begin(),spriteContexts[0].sprites.end(),sprite_delPred);
}
void moveSprites(int dx,int dy) {
	int numSelectedSprites = 0;
	int minX = 0x8000,minY = 0x8000;
	int maxX = 0,maxY = 0;
	//Check if any sprites are to be pasted
	for(int n=0; n<spriteContexts[0].sprites.size(); n++) {
		sprite_t thisSprite = spriteContexts[0].sprites[n];
		if(thisSprite.selected) {
			int xpos = thisSprite.data[2];
			int ypos = thisSprite.data[1]>>1;
			if(xpos<minX) minX = xpos;
			if(ypos<minY) minY = ypos;
			if(xpos>maxX) maxX = xpos;
			if(ypos>maxY) maxY = ypos;
			numSelectedSprites++;
		}
	}
	if(numSelectedSprites) {
		//Determine if any sprites will be out of bounds after this operation,
		//and if so, terminate
		//TODO
		//Move selected sprites
		//TODO
	}
}

///////////////////
//WINDOW FUNCTION//
///////////////////
HDC				hdcSp;
HBITMAP			hbmpSp;
DWORD *			bmpDataSp;

LRESULT CALLBACK WndProc_Sprite(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
	switch(msg) {
		//Creation and destruction of window(s)
		case WM_CREATE: {
			//Add controls
			//TODO
			//Create objects
			//TODO
			break;
		}
		case WM_DESTROY: {
			//Free objects
			//TODO
			break;
		}
		case WM_CLOSE: {
			//Simply hide the window
			ShowWindow(hwnd,SW_HIDE);
			wvisSprite = false;
			break;
		}
		//Updating
		case WM_PAINT: {
			//TODO
			break;
		}
		//Input
		case WM_COMMAND: {
			//TODO
			break;
		}
		default: {
			return DefWindowProc(hwnd,msg,wParam,lParam);
		}
	}
	return 0;
}

