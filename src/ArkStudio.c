
// src/ArkStudio.c
#define _GNU_SOURCE
#include "ArkStudio.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/types.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>

/* -------------------- Paramètres serveur -------------------- */

static volatile int server_running = 0;
static volatile int reload_flag = 0;
static pthread_t server_thread;
static uint16_t server_port = 8080; // surchargé par conf_start(port)

/* Écoute en parallèle sur les deux interfaces de BBB1 */
#define CONF_IFACE_ETH0 "192.168.0.101"
#define CONF_IFACE_USB0 "192.168.7.3"

/* Limites payload */
#define MAX_CFG_BODY (2048)
#define MAX_REQ      (8192)

/* -------------------- Journalisation (ring buffer) -------------------- */

typedef struct {
    char ts[32];       // "YYYY-MM-DD HH:MM:SS"
    char action[32];   // CONFIG_APPLY, CONFIG_JSON, TRIP_ON, TRIP_OFF, INFO...
    char detail[128];  // message détaillé
} conf_log_t;

#define CONF_LOG_RING_SZ 256
static conf_log_t mlogs[CONF_LOG_RING_SZ];
static int mlog_head=0, mlog_tail=0, mlog_count=0;
static pthread_mutex_t mlog_mtx = PTHREAD_MUTEX_INITIALIZER;

static void fmt_now(char out[32]) {
    time_t now = time(NULL);
    struct tm tm; localtime_r(&now, &tm);
    strftime(out, 32, "%Y-%m-%d %H:%M:%S", &tm);
}

static void _conf_log_nolock(const char* action, const char* detail) {
    if (mlog_count ==CONF_LOG_RING_SZ) {
        mlog_head = (mlog_head+1) % CONF_LOG_RING_SZ;
        mlog_count--;
    }
    int idx = mlog_tail;
    fmt_now(mlogs[idx].ts);
    strncpy(mlogs[idx].action, action ? action : "-", sizeof(mlogs[idx].action)-1);
    strncpy(mlogs[idx].detail, detail ? detail : "-", sizeof(mlogs[idx].detail)-1);
    mlogs[idx].action[sizeof(mlogs[idx].action)-1] = '\0';
    mlogs[idx].detail[sizeof(mlogs[idx].detail)-1] = '\0';
    mlog_tail = (mlog_tail+1) % CONF_LOG_RING_SZ;
    mlog_count++;
}

/* API publique */
void conf_add_log(const char* action, const char* detail) {
    pthread_mutex_lock(&mlog_mtx);
    _conf_log_nolock(action, detail);
    pthread_mutex_unlock(&mlog_mtx);
}

static void build_logs_json(char* out, size_t sz) {
    pthread_mutex_lock(&mlog_mtx);
    int i=0, idx=mlog_head;
    size_t off=0; off += snprintf(out+off, sz-off, "[\n");
    for (; i<mlog_count; ++i) {
        conf_log_t* e = &mlogs[idx];
        off += snprintf(out+off, sz-off,
                        " {\"ts\":\"%s\",\"action\":\"%s\",\"detail\":\"%s\"}%s\n",
                        e->ts, e->action, e->detail, (i==mlog_count-1?"":","));
        idx = (idx+1) % CONF_LOG_RING_SZ;
        if (off >= sz-64) break;
    }
    snprintf(out+off, sz-off, "]\n");
    pthread_mutex_unlock(&mlog_mtx);
}

/* -------------------- Helpers HTTP -------------------- */

static void send_http_response(int fd, int code, const char *ctype, const char *body) {
    char header[256];
    int body_len = (int)(body ? strlen(body) : 0);
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s; charset=UTF-8\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             code,
             (code == 200 ? "OK" :
              (code == 201 ? "Created" :
               (code == 302 ? "Found" :
                (code == 400 ? "Bad Request" :
                 (code == 404 ? "Not Found" : "Error"))))),
             ctype ? ctype : "text/plain",
             body_len);
    send(fd, header, strlen(header), 0);
    if (body && body_len > 0) send(fd, body, body_len, 0);
}

static void send_http_redirect(int fd, const char *location) {
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 302 Found\r\n"
             "Location: %s\r\n"
             "Content-Length: 0\r\n"
             "Connection: close\r\n\r\n", location);
    send(fd, header, strlen(header), 0);
}

static int write_atomic_json(const char *path, const char *json, size_t len) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "[ERROR] open(%s): %s\n", tmp, strerror(errno));
        return -1;
    }
    ssize_t w = write(fd, json, len);
    if (w < 0 || (size_t)w != len) {
        fprintf(stderr, "[ERROR] write(%s): %s\n", tmp, strerror(errno));
        close(fd);
        return -1;
    }
    if (fsync(fd) != 0) {
        fprintf(stderr, "[WARN] fsync(%s): %s\n", tmp, strerror(errno));
    }
    close(fd);
    if (rename(tmp, path) != 0) {
        fprintf(stderr, "[ERROR] rename(%s -> %s): %s\n", tmp, path, strerror(errno));
        return -1;
    }
    return 0;
}


static void build_current_config_json(char *buf, size_t sz) {
    snprintf(buf, sz,
        "{\n"
        "  \"threshold_A\": %.3f,\n"
        "  \"tms_A_ms\": %d,\n"
        "  \"threshold_V\": %.3f,\n"
        "  \"tms_V_ms\": %d,\n"
        "  \"samples\": %d,\n"
        "  \"sleep_between_samples_ms\": %d,\n"
        "  \"trip_logic\": \"%s\"\n"
        "}\n",
        config_get_threshold_A(),
        config_get_tms_A_ms(),
        config_get_threshold_V(),
        config_get_tms_V_ms(),
        config_get_samples(),
        config_get_sleep_ms(),
        config_get_trip_logic()
    );
}


static int is_probably_json(const char *s) {
    if (!s) return 0;
    while (*s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) s++;
    return *s == '{';
}

/* -------------------- IHM HTML (SCADA minimal) -------------------- */


static void render_home_html(char *out, size_t sz, const char *msg) {
    double thrA = config_get_threshold_A();
    int    tmsA = config_get_tms_A_ms();
    double thrV = config_get_threshold_V();
    int    tmsV = config_get_tms_V_ms();
    int    smp  = config_get_samples();
    int    slp  = config_get_sleep_ms();
    const char *logic = config_get_trip_logic(); // "any" ou "both"

    snprintf(out, sz,
        "<!DOCTYPE html><html lang=\"fr\"><head><meta charset=\"utf-8\"/>"
        "<title>BBB1 - SCADA config (CONF)</title>"
        "<style>"
        "body{font-family:Segoe UI,Arial;max-width:780px;margin:auto;padding:24px}"
        "label{display:block;margin:.6rem 0 .2rem}"
        "input,select{padding:.35rem;width:100%%}"
        "button{margin-top:1rem;padding:.5rem 1rem}"
        ".ok{color:green}.err{color:#b00}"
        "code{background:#f3f3f3;padding:.1rem .3rem;border-radius:4px}"
        "</style></head><body>"
        "<h1>SCADA - Configuration Protection (CONF)</h1>"
        "%s"
        "<form method=\"POST\" action=\"/apply\">"
        "<label>threshold_A</label>"
        "<input name=\"threshold_A\" type=\"number\" step=\"0.001\" value=\"%.3f\" required>"
        "<label>tms_A_ms</label>"
        "<input name=\"tms_A_ms\" type=\"number\" min=\"1\" max=\"600000\" value=\"%d\" required>"

        "<label>threshold_V</label>"
        "<input name=\"threshold_V\" type=\"number\" step=\"0.001\" value=\"%.3f\" required>"
        "<label>tms_V_ms</label>"
        "<input name=\"tms_V_ms\" type=\"number\" min=\"1\" max=\"600000\" value=\"%d\" required>"

        "<label>samples</label>"
        "<input name=\"samples\" type=\"number\" min=\"1\" max=\"128\" value=\"%d\" required>"
        "<label>sleep_between_samples_ms</label>"
        "<input name=\"sleep_between_samples_ms\" type=\"number\" min=\"0\" max=\"1000\" value=\"%d\" required>"

        "<label>trip_logic</label>"
        "<select name=\"trip_logic\">"
        "<option value=\"any\" %s>any</option>"
        "<option value=\"both\" %s>both</option>"
        "</select>"

        "<button type=\"submit\">Appliquer</button>"
        "</form>"
        "<hr>"
        "<p><small>API : <code>GET /config</code>, <code>POST /config</code> (JSON étendu), "
        "<code>GET /logs</code></small></p>"
        "</body></html>",
        (msg && *msg) ? msg : "",
        thrA, tmsA, thrV, tmsV, smp, slp,
        (logic && strcmp(logic,"any")==0) ? "selected" : "",
        (logic && strcmp(logic,"both")==0) ? "selected" : ""
    );
}

/* -------------------- Form-urlencoded parsing -------------------- */

static int hexval(char c){ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1;}
static void urldecode(char *s) {
    char *w=s; for (char *r=s; *r; ++r) {
        if (*r=='+' ){ *w=' '; w++; }
        else if (*r=='%' && isxdigit((unsigned char)r[1]) && isxdigit((unsigned char)r[2])) {
            int hi=hexval(r[1]); int lo=hexval(r[2]);
            if (hi>=0 && lo>=0){ *w=(char)((hi<<4)|lo); r+=2; w++; }
        } else { *w=*r; w++; }
    } *w='\0';
}
static int kv_get(const char *body, const char *key, char *out, size_t outsz) {
    size_t klen=strlen(key);
    const char *p=body;
    while((p=strstr(p,key))) {
        if ((p==body || p[-1]=='&') && p[klen]=='=') {
            p+=klen+1;
            size_t i=0;
            while (p[i] && p[i]!='&' && i+1<outsz) { out[i]=p[i]; i++; }
            out[i]='\0';
            urldecode(out);
            return 1;
        }
        p+=klen;
    }
    return 0;
}

/* -------------------- Thread HTTP (multi-interfaces) -------------------- */

static void* http_thread(void *arg) {
    (void)arg;

    int socks[2] = {-1, -1};
    const char* ifaces[2] = { CONF_IFACE_ETH0, CONF_IFACE_USB0 };

    for (int i = 0; i < 2; ++i) {
        socks[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (socks[i] < 0) { fprintf(stderr, "[ERROR] socket(%s): %s\n", ifaces[i], strerror(errno)); continue; }
        int opt = 1; setsockopt(socks[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in a; memset(&a,0,sizeof(a));
        a.sin_family = AF_INET; a.sin_port = htons(server_port); a.sin_addr.s_addr = inet_addr(ifaces[i]);
        if (bind(socks[i], (struct sockaddr*)&a, sizeof(a)) != 0) { fprintf(stderr,"[ERROR] bind %s:%u: %s\n", ifaces[i], server_port, strerror(errno)); close(socks[i]); socks[i]=-1; continue; }
        if (listen(socks[i], 8) != 0) { fprintf(stderr,"[ERROR] listen(%s): %s\n", ifaces[i], strerror(errno)); close(socks[i]); socks[i]=-1; continue; }
        char info[96]; snprintf(info,sizeof(info),"HTTP listening on %s:%u", ifaces[i], server_port);
        _conf_log_nolock("INFO", info); /* pas besoin de mutex ici avant la boucle */
        fprintf(stdout, "[INFO] %s\n", info);
    }
    if (socks[0] < 0 && socks[1] < 0) { fprintf(stderr, "[ERROR] Aucun socket HTTP démarré.\n"); server_running=0; return NULL; }

    while (server_running) {
        fd_set rfds; FD_ZERO(&rfds); int maxfd=-1;
        for (int i=0;i<2;++i) if (socks[i]>=0){ FD_SET(socks[i], &rfds); if (socks[i]>maxfd) maxfd=socks[i]; }
        struct timeval tv={.tv_sec=1,.tv_usec=0};
        int sel=select(maxfd+1,&rfds,NULL,NULL,&tv);
        if (sel<0){ if(errno==EINTR) continue; fprintf(stderr,"[WARN] select: %s\n",strerror(errno)); continue; }
        if (sel==0) continue;

        int fd=-1; struct sockaddr_in cli; socklen_t clilen=sizeof(cli);
        for (int i=0;i<2;++i){
            if (socks[i]>=0 && FD_ISSET(socks[i],&rfds)){
                fd=accept(socks[i],(struct sockaddr*)&cli,&clilen);
                if (fd<0){ if(errno==EINTR) continue; if(!server_running) break; fprintf(stderr,"[WARN] accept(%d): %s\n",i,strerror(errno)); continue; }
                break;
            }
        }
        if (fd<0) continue;

        char req[MAX_REQ];
        ssize_t r=recv(fd, req, sizeof(req)-1, 0);
        if (r<=0){ close(fd); continue; }
        req[r]='\0';

        char method[8]={0}; char path[64]={0};
        sscanf(req,"%7s %63s", method, path);

        int content_length=0;
        char *cl=strcasestr(req,"Content-Length:");
        if (cl) sscanf(cl,"Content-Length: %d",&content_length);

        char *hdr_end=strstr(req,"\r\n\r\n");
        char *body = hdr_end ? (hdr_end+4) : NULL;

        /* GET / -> IHM HTML */
        if (strcmp(method,"GET")==0 && strcmp(path,"/")==0){
            char page[4096];
            render_home_html(page,sizeof(page), "");
            send_http_response(fd, 200, "text/html", page);
            close(fd);
            continue;
        }

        /* GET /config -> JSON */
        if (strcmp(method,"GET")==0 && strcmp(path,"/config")==0){
            char json[256];
            build_current_config_json(json, sizeof(json));
            send_http_response(fd, 200, "application/json", json);
            close(fd);
            continue;
        }

        /* GET /logs -> JSON */
        if (strcmp(method,"GET")==0 && strcmp(path,"/logs")==0){
            char js[8192];
            build_logs_json(js,sizeof(js));
            send_http_response(fd, 200, "application/json", js);
            close(fd);
            continue;
        }



/* POST /apply -> formulaire HTML (x-www-form-urlencoded) */
if (strcmp(method,"POST")==0 && strcmp(path,"/apply")==0) {
    if (content_length <= 0 || content_length > MAX_CFG_BODY) {
        char page[4096]; render_home_html(page,sizeof(page), "<p class='err'>Payload invalide.</p>");
        send_http_response(fd, 400, "text/html", page);
        close(fd);
        continue;
    }

    size_t have=0; char bufp[MAX_CFG_BODY+1];
    if (body) {
        size_t in_first=(size_t)(r - (body - req));
        if (in_first > (size_t)content_length) in_first=(size_t)content_length;
        memcpy(bufp, body, in_first); have=in_first;
    }
    while (have < (size_t)content_length) {
        ssize_t rr=recv(fd, bufp+have, (size_t)content_length-have, 0);
        if (rr <= 0) { break; }
        have += (size_t)rr;
    }
    if (have != (size_t)content_length) {
        char page[4096]; render_home_html(page,sizeof(page), "<p class='err'>Body incomplet.</p>");
        send_http_response(fd, 400, "text/html", page);
        close(fd);
        continue;
    }
    bufp[content_length]='\0';

    /* parse des champs étendus */
    char s_thrA[64]={0}, s_tmsA[32]={0}, s_thrV[64]={0}, s_tmsV[32]={0};
    char s_smp[32]={0}, s_slp[32]={0}, s_logic[16]={0};

    int ok = 1;
    ok &= kv_get(bufp,"threshold_A", s_thrA,sizeof(s_thrA));
    ok &= kv_get(bufp,"tms_A_ms",   s_tmsA,sizeof(s_tmsA));
    ok &= kv_get(bufp,"threshold_V", s_thrV,sizeof(s_thrV));
    ok &= kv_get(bufp,"tms_V_ms",   s_tmsV,sizeof(s_tmsV));
    ok &= kv_get(bufp,"samples",    s_smp, sizeof(s_smp));
    ok &= kv_get(bufp,"sleep_between_samples_ms", s_slp, sizeof(s_slp));
    ok &= kv_get(bufp,"trip_logic", s_logic, sizeof(s_logic));

    if (!ok) {
        char page[4096]; render_home_html(page,sizeof(page), "<p class='err'>Champs manquants.</p>");
        send_http_response(fd, 400, "text/html", page);
        close(fd);
        continue;
    }

    /* validations rapides */
    double thrA = atof(s_thrA);
    int    tmsA = atoi(s_tmsA);
    double thrV = atof(s_thrV);
    int    tmsV = atoi(s_tmsV);
    int    smp  = atoi(s_smp);
    int    slp  = atoi(s_slp);
    urldecode(s_logic); // pour sûreté

    int logic_ok = (strcmp(s_logic,"any")==0 || strcmp(s_logic,"both")==0);

    if (!(thrA >= 0.0 && thrV >= 0.0 &&
          tmsA > 0 && tmsV > 0 &&
          smp >= 1 && smp <= 128 &&
          slp >= 0 && slp <= 1000 &&
          logic_ok)) {
        char page[4096]; render_home_html(page,sizeof(page), "<p class='err'>Valeurs invalides.</p>");
        send_http_response(fd, 400, "text/html", page);
        close(fd);
        continue;
    }

    /* fabrique le JSON étendu exact */
    char json[256];
    int n = snprintf(json,sizeof(json),
        "{\n"
        "  \"threshold_A\": %.3f,\n"
        "  \"tms_A_ms\": %d,\n"
        "  \"threshold_V\": %.3f,\n"
        "  \"tms_V_ms\": %d,\n"
        "  \"samples\": %d,\n"
        "  \"sleep_between_samples_ms\": %d,\n"
        "  \"trip_logic\": \"%s\"\n"
        "}\n",
        thrA, tmsA, thrV, tmsV, smp, slp, s_logic
    );
    if (n <= 0 || n >= (int)sizeof(json)) {
        char page[4096]; render_home_html(page,sizeof(page), "<p class='err'>Construction JSON impossible.</p>");
        send_http_response(fd, 400, "text/html", page);
        close(fd);
        continue;
    }

    if (write_atomic_json("config.json", json, (size_t)n) != 0) {
        char page[4096]; render_home_html(page,sizeof(page), "<p class='err'>Écriture fichier échouée.</p>");
        send_http_response(fd, 400, "text/html", page);
        close(fd);
        continue;
    }

    reload_flag = 1;
    char info[160];
    snprintf(info,sizeof(info),
             "CONFIG_APPLY_EXT thrA=%.3f tmsA=%d thrV=%.3f tmsV=%d smp=%d slp=%d logic=%s",
             thrA, tmsA, thrV, tmsV, smp, slp, s_logic);
    conf_add_log("CONFIG_APPLY_EXT", info);

    /* redirection vers l'accueil */
    send_http_redirect(fd, "/");
    close(fd);
    continue;
}


        /* POST /config -> JSON (API existante) */
        if (strcmp(method,"POST")==0 && strcmp(path,"/config")==0){
            if (content_length <= 0 || content_length > MAX_CFG_BODY) { send_http_response(fd,400,"text/plain","Missing/Too large JSON body\n"); close(fd); continue; }
            size_t have=0; char bufj[MAX_CFG_BODY+1];
            if (body){
                size_t in_first=(size_t)(r - (body - req));
                if (in_first>(size_t)content_length) in_first=(size_t)content_length;
                memcpy(bufj, body, in_first); have=in_first;
            }
            while (have < (size_t)content_length){
                ssize_t rr=recv(fd, bufj+have,(size_t)content_length-have,0);
                if (rr <= 0) { break; }
                have += (size_t)rr;
            }
            if (have != (size_t)content_length){ send_http_response(fd,400,"text/plain","Incomplete body\n"); close(fd); continue; }
            bufj[content_length]='\0';
            if (!is_probably_json(bufj)){ send_http_response(fd,400,"text/plain","Invalid JSON\n"); close(fd); continue; }
            if (write_atomic_json("config.json", bufj, (size_t)content_length) != 0){ send_http_response(fd,400,"text/plain","Write failed\n"); close(fd); continue; }
            reload_flag = 1;
            conf_add_log("CONFIG_JSON", "config updated via JSON");
            send_http_response(fd, 201, "application/json", "{\"status\":\"updated\"}\n");
            close(fd);
            continue;
        }

        /* 404 par défaut */
        send_http_response(fd, 404, "text/plain", "Not Found\n");
        close(fd);
    }

    for (int i=0;i<2;++i) if (socks[i]>=0) close(socks[i]);
    fprintf(stdout, "[INFO] CONF HTTP arrêté.\n");
    return NULL;
}

/* -------------------- API publique -------------------- */

int conf_start(uint16_t port) {
    if (server_running) return 0;
    server_port = port;
    server_running = 1;
    int rc = pthread_create(&server_thread, NULL, http_thread, NULL);
    if (rc != 0) {
        fprintf(stderr, "[ERROR] pthread_create: %s\n", strerror(errno));
        server_running = 0;
        return -1;
    }
    return 0;
}

void conf_stop(void) {
    if (!server_running) return;
    server_running = 0;

    /* Poke: connecte sur les deux IP pour réveiller select/accept */
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s >= 0) {
        struct sockaddr_in a; memset(&a,0,sizeof(a));
        a.sin_family = AF_INET; a.sin_port = htons(server_port);
        a.sin_addr.s_addr = inet_addr(CONF_IFACE_ETH0); (void)connect(s,(struct sockaddr*)&a,sizeof(a));
        a.sin_addr.s_addr = inet_addr(CONF_IFACE_USB0); (void)connect(s,(struct sockaddr*)&a,sizeof(a));
        close(s);
    }
    pthread_join(server_thread, NULL);
}

int conf_need_reload(void) { return reload_flag ? 1 : 0; }
void conf_clear_reload_flag(void) { reload_flag = 0; }
