// Iker's Music 3DS - reproductor de musica con playlists para Nintendo 3DS (homebrew)
#include <3ds.h>
#include <citro2d.h>
#include <mpg123.h>
#include <tremor/ivorbisfile.h>
#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>

#define MUSIC_DIR "sdmc:/music"
#define DATA_DIR "sdmc:/3ds/IkersMusic"
#define PL_DIR DATA_DIR "/playlists"
#define MAX_SONGS 3000
#define MAX_PLAYLISTS 64
#define NBUF 4
#define BUF_FRAMES 8192
#define ROWS 8
#define ROW_H 22

// ---------- datos ----------
typedef struct { char path[256]; char name[96]; } Song;
typedef struct { char name[48]; int *idx; int count; } Playlist;

static Song *lib; static int libCount;
static Playlist pls[MAX_PLAYLISTS]; static int plCount;

// ---------- decodificador ----------
enum { DEC_NONE, DEC_MP3, DEC_OGG, DEC_WAV };
typedef struct {
	int type, ch, rate; long long totalFrames;
	mpg123_handle *mh; OggVorbis_File vf; FILE *f; long dataLeft;
} Dec;
static Dec dec;

static bool hasExt(const char *n, const char *e) {
	size_t a = strlen(n), b = strlen(e);
	if (a < b) return false;
	for (size_t i = 0; i < b; i++) if (tolower((unsigned char)n[a - b + i]) != e[i]) return false;
	return true;
}

static void decClose(void) {
	if (dec.type == DEC_MP3 && dec.mh) { mpg123_close(dec.mh); mpg123_delete(dec.mh); }
	if (dec.type == DEC_OGG) ov_clear(&dec.vf);
	else if (dec.f) fclose(dec.f);
	memset(&dec, 0, sizeof dec);
}

static bool decOpen(const char *path) {
	decClose();
	if (hasExt(path, ".mp3")) {
		int err; dec.mh = mpg123_new(NULL, &err);
		if (!dec.mh) return false;
		mpg123_format_none(dec.mh);
		static const long rates[] = {8000,11025,12000,16000,22050,24000,32000,44100,48000};
		for (unsigned i = 0; i < sizeof rates / sizeof *rates; i++)
			mpg123_format(dec.mh, rates[i], MPG123_STEREO, MPG123_ENC_SIGNED_16);
		if (mpg123_open(dec.mh, path) != MPG123_OK) { mpg123_delete(dec.mh); dec.mh = NULL; return false; }
		long r; int c, e;
		if (mpg123_getformat(dec.mh, &r, &c, &e) != MPG123_OK) { mpg123_close(dec.mh); mpg123_delete(dec.mh); dec.mh = NULL; return false; }
		dec.type = DEC_MP3; dec.rate = (int)r; dec.ch = 2;
		off_t len = mpg123_length(dec.mh);
		dec.totalFrames = len > 0 ? len : 0;
		return true;
	}
	if (hasExt(path, ".ogg")) {
		FILE *of = fopen(path, "rb"); if (!of) return false;
		if (ov_open(of, &dec.vf, NULL, 0) < 0) { fclose(of); return false; }
		vorbis_info *vi = ov_info(&dec.vf, -1);
		if (!vi || vi->channels < 1 || vi->channels > 2) { ov_clear(&dec.vf); return false; }
		dec.type = DEC_OGG; dec.rate = (int)vi->rate; dec.ch = vi->channels;
		dec.totalFrames = (long long)ov_time_total(&dec.vf, -1) * dec.rate / 1000;
		return true;
	}
	if (hasExt(path, ".wav")) {
		FILE *f = fopen(path, "rb"); if (!f) return false;
		unsigned char h[12]; if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fclose(f); return false; }
		int ch = 0, rate = 0, bits = 0, fmt = 0;
		for (;;) {
			unsigned char ck[8]; if (fread(ck, 1, 8, f) != 8) { fclose(f); return false; }
			long sz = ck[4] | ck[5] << 8 | ck[6] << 16 | (long)ck[7] << 24;
			if (!memcmp(ck, "fmt ", 4)) {
				unsigned char b[16]; if (sz < 16 || fread(b, 1, 16, f) != 16) { fclose(f); return false; }
				fmt = b[0] | b[1] << 8; ch = b[2] | b[3] << 8;
				rate = b[4] | b[5] << 8 | b[6] << 16 | b[7] << 24; bits = b[14] | b[15] << 8;
				fseek(f, sz - 16 + (sz & 1), SEEK_CUR);
			} else if (!memcmp(ck, "data", 4)) {
				if (fmt != 1 || bits != 16 || ch < 1 || ch > 2) { fclose(f); return false; }
				dec.f = f; dec.type = DEC_WAV; dec.ch = ch; dec.rate = rate; dec.dataLeft = sz;
				dec.totalFrames = sz / (2 * ch);
				return true;
			} else fseek(f, sz + (sz & 1), SEEK_CUR);
		}
	}
	return false;
}

// devuelve frames decodificados (0 = fin)
static int decRead(s16 *out, int maxFrames) {
	int bytesPerFrame = 2 * dec.ch, want = maxFrames * bytesPerFrame;
	switch (dec.type) {
	case DEC_MP3: {
		size_t done = 0; int r = mpg123_read(dec.mh, (unsigned char *)out, want, &done);
		(void)r;
		return (int)(done / bytesPerFrame);
	}
	case DEC_OGG: {
		int got = 0, bs;
		while (got < want) {
			long r = ov_read(&dec.vf, (char *)out + got, want - got, &bs);
			if (r == 0) break;
			if (r < 0) continue; // OV_HOLE: seguir
			got += (int)r;
		}
		return got / bytesPerFrame;
	}
	case DEC_WAV: {
		long n = want < dec.dataLeft ? want : dec.dataLeft;
		size_t r = fread(out, 1, n, dec.f); dec.dataLeft -= r;
		return (int)(r / bytesPerFrame);
	}
	}
	return 0;
}

// ---------- reproductor ----------
static ndspWaveBuf wbuf[NBUF]; static s16 *pcm; static bool queued[NBUF];
static bool audioOk, playing, paused, decEof, shuffle; static int repeatMode; // 0 off,1 todo,2 uno
static int *queue, qCount, qPos; static long long playedFrames;
static float volume = 0.8f; static char status[96] = "";

static void stopPlayback(void) {
	ndspChnWaveBufClear(0);
	for (int i = 0; i < NBUF; i++) queued[i] = false;
	decClose(); playing = paused = false; decEof = false; playedFrames = 0;
}

static bool startTrack(int songIdx) {
	stopPlayback();
	if (!audioOk) { snprintf(status, sizeof status, "DSP no disponible (falta dspfirm.cdc)"); return false; }
	if (!decOpen(lib[songIdx].path)) { snprintf(status, sizeof status, "No se pudo abrir: %s", lib[songIdx].name); return false; }
	ndspChnReset(0);
	ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
	ndspChnSetRate(0, (float)dec.rate);
	ndspChnSetFormat(0, dec.ch == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
	float mix[12] = {1, 1};
	ndspChnSetMix(0, mix);
	for (int i = 0; i < NBUF; i++) { memset(&wbuf[i], 0, sizeof wbuf[i]); wbuf[i].data_vaddr = pcm + i * BUF_FRAMES * 2; }
	playing = true; status[0] = 0;
	return true;
}

static void playQueueAt(int pos) {
	if (pos < 0 || pos >= qCount) { stopPlayback(); return; }
	qPos = pos;
	if (!startTrack(queue[pos])) playing = false;
}

static void nextTrack(bool auto_) {
	if (!qCount) return;
	if (auto_ && repeatMode == 2) { playQueueAt(qPos); return; }
	int n;
	if (shuffle && qCount > 1) { do n = rand() % qCount; while (n == qPos); }
	else n = qPos + 1;
	if (n >= qCount) { if (repeatMode == 1 || !auto_) n = 0; else { stopPlayback(); return; } }
	playQueueAt(n);
}

static void prevTrack(void) {
	if (!qCount) return;
	if (playedFrames > (long long)dec.rate * 3) { playQueueAt(qPos); return; }
	playQueueAt(qPos > 0 ? qPos - 1 : qCount - 1);
}

static void setQueue(const int *src, int n, int start) {
	free(queue); queue = malloc(sizeof(int) * (n ? n : 1));
	memcpy(queue, src, sizeof(int) * n); qCount = n;
	playQueueAt(start);
}

static void togglePause(void) {
	if (!playing) return;
	paused = !paused; ndspChnSetPaused(0, paused);
}

static void audioUpdate(void) {
	if (!playing) return;
	for (int i = 0; i < NBUF; i++) {
		if (queued[i] && wbuf[i].status == NDSP_WBUF_DONE) { playedFrames += wbuf[i].nsamples; queued[i] = false; }
		if (!queued[i] && !decEof) {
			s16 *p = pcm + i * BUF_FRAMES * 2;
			int n = decRead(p, BUF_FRAMES);
			if (n <= 0) { decEof = true; break; }
			wbuf[i].nsamples = n; wbuf[i].data_vaddr = p;
			DSP_FlushDataCache(p, n * dec.ch * 2);
			ndspChnWaveBufAdd(0, &wbuf[i]); queued[i] = true;
		}
	}
	if (decEof) {
		bool any = false;
		for (int i = 0; i < NBUF; i++) if (queued[i] && wbuf[i].status != NDSP_WBUF_DONE) any = true;
		if (!any) nextTrack(true);
	}
}

// ---------- biblioteca / playlists ----------
static void trimName(char *dst, const char *file, size_t cap) {
	const char *s = strrchr(file, '/'); s = s ? s + 1 : file;
	strncpy(dst, s, cap - 1); dst[cap - 1] = 0;
	char *dot = strrchr(dst, '.'); if (dot) *dot = 0;
}

static void scanDir(const char *dir, int depth) {
	DIR *d = opendir(dir); if (!d) return;
	struct dirent *e;
	while ((e = readdir(d)) && libCount < MAX_SONGS) {
		if (e->d_name[0] == '.') continue;
		char p[256]; snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
		if (e->d_type == DT_DIR) { if (depth < 4) scanDir(p, depth + 1); }
		else if (hasExt(e->d_name, ".mp3") || hasExt(e->d_name, ".ogg") || hasExt(e->d_name, ".wav")) {
			Song *s = &lib[libCount++];
			strncpy(s->path, p, sizeof s->path - 1); trimName(s->name, e->d_name, sizeof s->name);
		}
	}
	closedir(d);
}

static int cmpSong(const void *a, const void *b) { return strcasecmp(((Song *)a)->name, ((Song *)b)->name); }

static int findSong(const char *path) {
	for (int i = 0; i < libCount; i++) if (!strcmp(lib[i].path, path)) return i;
	return -1;
}

static void plSave(Playlist *p) {
	char f[160]; snprintf(f, sizeof f, "%s/%s.m3u", PL_DIR, p->name);
	FILE *fp = fopen(f, "w"); if (!fp) return;
	for (int i = 0; i < p->count; i++) fprintf(fp, "%s\n", lib[p->idx[i]].path);
	fclose(fp);
}

static void plLoadAll(void) {
	for (int i = 0; i < plCount; i++) free(pls[i].idx);
	plCount = 0;
	DIR *d = opendir(PL_DIR); if (!d) return;
	struct dirent *e;
	while ((e = readdir(d)) && plCount < MAX_PLAYLISTS) {
		if (!hasExt(e->d_name, ".m3u")) continue;
		Playlist *p = &pls[plCount++]; memset(p, 0, sizeof *p);
		trimName(p->name, e->d_name, sizeof p->name);
		char f[160]; snprintf(f, sizeof f, "%s/%s", PL_DIR, e->d_name);
		FILE *fp = fopen(f, "r"); if (!fp) continue;
		char line[300]; int cap = 16; p->idx = malloc(sizeof(int) * cap);
		while (fgets(line, sizeof line, fp)) {
			line[strcspn(line, "\r\n")] = 0;
			int s = findSong(line); if (s < 0) continue;
			if (p->count == cap) { cap *= 2; p->idx = realloc(p->idx, sizeof(int) * cap); }
			p->idx[p->count++] = s;
		}
		fclose(fp);
	}
	closedir(d);
}

static bool askText(const char *hint, char *out, size_t cap) {
	SwkbdState kb; swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, (int)cap - 1);
	swkbdSetHintText(&kb, hint); 
	swkbdSetValidation(&kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
	return swkbdInputText(&kb, out, cap) == SWKBD_BUTTON_CONFIRM;
}

static void plCreate(void) {
	if (plCount >= MAX_PLAYLISTS) return;
	char name[48] = "";
	if (!askText("Nombre de la playlist", name, sizeof name)) return;
	for (char *c = name; *c; c++) if (strchr("/\\:*?\"<>|.", *c)) *c = '_';
	for (int i = 0; i < plCount; i++) if (!strcasecmp(pls[i].name, name)) return;
	Playlist *p = &pls[plCount++]; memset(p, 0, sizeof *p);
	strncpy(p->name, name, sizeof p->name - 1); p->idx = malloc(sizeof(int) * 16);
	plSave(p);
}

static void plDelete(int i) {
	char f[160]; snprintf(f, sizeof f, "%s/%s.m3u", PL_DIR, pls[i].name); remove(f);
	free(pls[i].idx);
	memmove(&pls[i], &pls[i + 1], sizeof(Playlist) * (plCount - i - 1)); plCount--;
}

static void plAdd(Playlist *p, int song) {
	for (int i = 0; i < p->count; i++) if (p->idx[i] == song) return;
	if (p->count % 16 == 0) p->idx = realloc(p->idx, sizeof(int) * (p->count + 16));
	p->idx[p->count++] = song; plSave(p);
}

static void plRemoveAt(Playlist *p, int i) {
	memmove(&p->idx[i], &p->idx[i + 1], sizeof(int) * (p->count - i - 1)); p->count--; plSave(p);
}

// ---------- interfaz ----------
enum { V_LIB, V_PLS, V_PLSONGS, V_PICK };
static int view = V_LIB, sel, scroll, curPl, pickSong;
static C2D_TextBuf tbuf;
static const u32 CBG = 0xFF30180F, CPANEL = 0xFF50301A, CSEL = 0xFFC88A1E, CACC = 0xFFFFC832, CTXT = 0xFFFFFFFF, CDIM = 0xFFB4A096;

static void txt(const char *s, float x, float y, float scale, u32 col, size_t maxc) {
	char b[128]; size_t n = strlen(s); if (maxc > 120) maxc = 120;
	if (n > maxc) { n = maxc; while (n && ((unsigned char)s[n] & 0xC0) == 0x80) n--; }
	memcpy(b, s, n); b[n] = 0;
	C2D_Text t; C2D_TextParse(&t, tbuf, b); C2D_TextOptimize(&t);
	C2D_DrawText(&t, C2D_WithColor, x, y, 0.5f, scale, scale, col);
}

static void mmss(char *o, long long sec) { snprintf(o, 16, "%d:%02d", (int)(sec / 60), (int)(sec % 60)); }

static int listCount(void) {
	switch (view) { case V_LIB: return libCount; case V_PLS: case V_PICK: return plCount; default: return pls[curPl].count; }
}

static void listLabel(int i, char *o, size_t cap) {
	switch (view) {
	case V_LIB: snprintf(o, cap, "%s", lib[i].name); break;
	case V_PLS: case V_PICK: snprintf(o, cap, "%s (%d)", pls[i].name, pls[i].count); break;
	default: snprintf(o, cap, "%s", lib[pls[curPl].idx[i]].name);
	}
}

static void drawTop(void) {
	C2D_DrawRectSolid(0, 0, 0, 400, 240, CBG);
	txt("Iker's Music 3DS", 12, 6, 0.7f, CACC, 40);
	C2D_DrawRectSolid(12, 34, 0, 376, 2, CACC);
	if (playing && qCount) {
		const Song *s = &lib[queue[qPos]];
		txt(paused ? "|| En pausa" : "> Reproduciendo", 12, 46, 0.5f, CDIM, 40);
		txt(s->name, 12, 72, 0.8f, CTXT, 34);
		long long el = dec.rate ? playedFrames / dec.rate : 0, tot = dec.rate ? dec.totalFrames / dec.rate : 0;
		char a[16], b[16], line[40]; mmss(a, el); mmss(b, tot); snprintf(line, sizeof line, "%s / %s", a, tot ? b : "--:--");
		txt(line, 12, 118, 0.6f, CTXT, 30);
		C2D_DrawRectSolid(12, 146, 0, 376, 8, CPANEL);
		float f = tot ? (float)el / tot : 0; if (f > 1) f = 1;
		C2D_DrawRectSolid(12, 146, 0, 376 * f, 8, CACC);
		char q[48]; snprintf(q, sizeof q, "Pista %d de %d", qPos + 1, qCount); txt(q, 12, 164, 0.5f, CDIM, 40);
	} else {
		txt(status[0] ? status : "Nada en reproduccion", 12, 70, 0.6f, CDIM, 44);
		txt("Pon tu musica (mp3/ogg/wav) en sdmc:/music", 12, 100, 0.5f, CDIM, 50);
	}
	char vol[40]; snprintf(vol, sizeof vol, "Vol %d%%   %s   %s", (int)(volume * 100 + 0.5f), shuffle ? "Aleatorio: SI" : "Aleatorio: no",
		repeatMode == 0 ? "Repetir: no" : repeatMode == 1 ? "Repetir: todo" : "Repetir: una");
	txt(vol, 12, 192, 0.5f, CTXT, 60);
	txt("A:tocar X:pausa Y:playlist L/R:ant/sig <>:vol", 12, 214, 0.4f, CDIM, 70);
}

static const char *tabs[] = {"Canciones", "Playlists"};
static const char *btns[] = {"|<", "||", ">|", "Shuf", "Rep"};

static void drawBottom(void) {
	C2D_DrawRectSolid(0, 0, 0, 320, 240, CBG);
	for (int t = 0; t < 2; t++) {
		bool on = (t == 0 && view == V_LIB) || (t == 1 && view != V_LIB);
		C2D_DrawRectSolid(t * 160, 0, 0, 160, 24, on ? CSEL : CPANEL);
		txt(tabs[t], t * 160 + 40, 3, 0.55f, CTXT, 14);
	}
	char hdr[64] = "";
	if (view == V_PLSONGS) snprintf(hdr, sizeof hdr, "< %s", pls[curPl].name);
	else if (view == V_PICK) snprintf(hdr, sizeof hdr, "Anadir a que playlist?");
	else if (view == V_PLS) snprintf(hdr, sizeof hdr, "Y: nueva  SELECT: borrar");
	if (hdr[0]) txt(hdr, 6, 26, 0.45f, CACC, 40);
	int n = listCount();
	if (sel >= n) sel = n ? n - 1 : 0;
	if (sel < scroll) scroll = sel;
	if (sel >= scroll + ROWS) scroll = sel - ROWS + 1;
	for (int r = 0; r < ROWS && scroll + r < n; r++) {
		int i = scroll + r; float y = 42 + r * ROW_H;
		if (i == sel) C2D_DrawRectSolid(0, y, 0, 320, ROW_H - 1, CSEL);
		char l[128]; listLabel(i, l, sizeof l); txt(l, 8, y + 2, 0.5f, CTXT, 44);
	}
	if (!n) txt(view == V_LIB ? "Sin canciones en sdmc:/music" : "Vacio", 8, 60, 0.5f, CDIM, 40);
	for (int b = 0; b < 5; b++) {
		C2D_DrawRectSolid(b * 64 + 2, 218, 0, 60, 20, CPANEL);
		txt(btns[b], b * 64 + 12, 220, 0.45f, CTXT, 6);
	}
}

static void activate(void) {
	int n = listCount(); if (!n) return;
	switch (view) {
	case V_LIB: { int *all = malloc(sizeof(int) * libCount); for (int i = 0; i < libCount; i++) all[i] = i; setQueue(all, libCount, sel); free(all); break; }
	case V_PLS: curPl = sel; view = V_PLSONGS; sel = scroll = 0; break;
	case V_PLSONGS: setQueue(pls[curPl].idx, pls[curPl].count, sel); break;
	case V_PICK: plAdd(&pls[sel], pickSong); snprintf(status, sizeof status, "Anadida a %s", pls[sel].name); view = V_LIB; sel = pickSong; scroll = 0; break;
	}
}

static void goTab(int t) {
	if (t == 0 && view != V_LIB) { view = V_LIB; sel = scroll = 0; }
	if (t == 1 && view == V_LIB) { view = V_PLS; sel = scroll = 0; }
}

static void back(void) {
	if (view == V_PLSONGS) { view = V_PLS; sel = curPl; scroll = 0; }
	else if (view == V_PICK) { view = V_LIB; sel = pickSong; scroll = 0; }
}

static void setVolume(float v) { volume = v < 0 ? 0 : v > 1 ? 1 : v; if (audioOk) ndspSetMasterVol(volume); }

int main(void) {
	srand(time(NULL));
	gfxInitDefault(); C3D_Init(C3D_DEFAULT_CMDBUF_SIZE); C2D_Init(C2D_DEFAULT_MAX_OBJECTS); C2D_Prepare();
	C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT), *bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	tbuf = C2D_TextBufNew(8192);

	mkdir("sdmc:/3ds", 0777); mkdir(DATA_DIR, 0777); mkdir(PL_DIR, 0777); mkdir(MUSIC_DIR, 0777);
	mpg123_init();
	audioOk = R_SUCCEEDED(ndspInit());
	if (audioOk) { ndspSetOutputMode(NDSP_OUTPUT_STEREO); ndspSetMasterVol(volume); }
	pcm = linearAlloc(sizeof(s16) * 2 * BUF_FRAMES * NBUF);
	lib = calloc(MAX_SONGS, sizeof(Song));
	scanDir(MUSIC_DIR, 0);
	qsort(lib, libCount, sizeof(Song), cmpSong);
	plLoadAll();
	if (!audioOk) snprintf(status, sizeof status, "Falta sdmc:/3ds/dspfirm.cdc (DSP1)");

	while (aptMainLoop()) {
		hidScanInput();
		u32 kd = hidKeysDown(), kh = hidKeysHeld();
		if (kd & KEY_START) break;
		int n = listCount();
		if (kd & KEY_DOWN) { if (n) sel = (sel + 1) % n; }
		if (kd & KEY_UP) { if (n) sel = (sel + n - 1) % n; }
		if (kd & KEY_CPAD_DOWN) sel = sel + 1 < n ? sel + 1 : sel;
		if (kd & KEY_CPAD_UP) sel = sel > 0 ? sel - 1 : sel;
		if (kd & KEY_A) activate();
		if (kd & KEY_B) back();
		if (kd & KEY_X) togglePause();
		if (kd & KEY_L) prevTrack();
		if (kd & KEY_R) nextTrack(false);
		if (kd & KEY_ZL) goTab(0);
		if (kd & KEY_ZR) goTab(1);
		if (kh & KEY_DLEFT) setVolume(volume - 0.01f);
		if (kh & KEY_DRIGHT) setVolume(volume + 0.01f);
		if (kd & KEY_Y) {
			if (view == V_LIB && libCount) { if (plCount) { pickSong = sel; view = V_PICK; sel = scroll = 0; } else snprintf(status, sizeof status, "Crea una playlist primero (pestana Playlists)"); }
			else if (view == V_PLS) plCreate();
			else if (view == V_PLSONGS && pls[curPl].count) { plRemoveAt(&pls[curPl], sel); }
		}
		if ((kd & KEY_SELECT) && view == V_PLS && plCount) { plDelete(sel); if (sel >= plCount) sel = plCount ? plCount - 1 : 0; }
		if (kd & KEY_TOUCH) {
			touchPosition tp; hidTouchRead(&tp);
			if (tp.py < 24) goTab(tp.px / 160);
			else if (tp.py >= 218) {
				switch (tp.px / 64) {
				case 0: prevTrack(); break; case 1: togglePause(); break; case 2: nextTrack(false); break;
				case 3: shuffle = !shuffle; break; case 4: repeatMode = (repeatMode + 1) % 3; break;
				}
			} else if (tp.py >= 42) {
				int r = (tp.py - 42) / ROW_H;
				if (scroll + r < n) { if (sel == scroll + r) activate(); else sel = scroll + r; }
			}
		}
		audioUpdate();

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TextBufClear(tbuf);
		C2D_TargetClear(top, CBG); C2D_SceneBegin(top); drawTop();
		C2D_TargetClear(bot, CBG); C2D_SceneBegin(bot); drawBottom();
		C3D_FrameEnd(0);
	}

	stopPlayback();
	if (audioOk) ndspExit();
	linearFree(pcm); mpg123_exit();
	C2D_TextBufDelete(tbuf); C2D_Fini(); C3D_Fini(); gfxExit();
	return 0;
}
