#include <stdio.h>
#include <string.h>

#include "mongoose.h"
#include "http.h"
#include "exercises.h"
#include "workouts.h"
#include "stats.h"
#include "admin.h"
#include "auth.h"

// ---------- Helpers HTTP ----------
int is_get(struct mg_http_message *hm)    { return mg_match(hm->method, mg_str("GET"), NULL); }
int is_post(struct mg_http_message *hm)   { return mg_match(hm->method, mg_str("POST"), NULL); }
int is_put(struct mg_http_message *hm)    { return mg_match(hm->method, mg_str("PUT"), NULL); }
int is_delete(struct mg_http_message *hm) { return mg_match(hm->method, mg_str("DELETE"), NULL); }

// ---------- Handlers genéricos ----------
void handle_health(struct mg_connection *c) {
  mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                "{ \"status\": \"ok\" }\n");
}

void handle_not_found(struct mg_connection *c) {
  mg_http_reply(c, 404, "Content-Type: application/json\r\n",
                "{ \"error\": \"not found\" }\n");
}

// ---------- Static files ----------
static int serve_static(struct mg_connection *c, struct mg_http_message *hm) {
  struct mg_http_serve_opts opts = {
    .root_dir = "public",
    .fs = &mg_fs_posix
  };

  // "/" -> index.html
  if (mg_match(hm->uri, mg_str("/"), NULL)) {
    mg_http_serve_file(c, hm, "public/index.html", &opts);  // <-- const char*
    return 1;
  }

  // Páginas e assets
  if (mg_match(hm->uri, mg_str("/index.html"), NULL) ||
      mg_match(hm->uri, mg_str("/dashboard.html"), NULL) ||
      mg_match(hm->uri, mg_str("/admin.html"), NULL) ||
      mg_match(hm->uri, mg_str("/css/#"), NULL) ||
      mg_match(hm->uri, mg_str("/js/#"), NULL)) {
    mg_http_serve_dir(c, hm, &opts);
    return 1;
  }

  return 0;
}

// ---------- Router ----------
void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev != MG_EV_HTTP_MSG) return;

  struct mg_http_message *hm = (struct mg_http_message *) ev_data;

  // Servir frontend (antes da API)
  if (is_get(hm) && serve_static(c, hm)) return;

  // 1) Público: /health
  if (is_get(hm) && mg_match(hm->uri, mg_str("/health"), NULL)) {
    handle_health(c);
    return;
  }

  // 2) Público: /login
  if (is_post(hm) && mg_match(hm->uri, mg_str("/login"), NULL)) {
    handle_post_login(c, hm);
    return;
  }

  // 3) Público: /signup (auto-login no handler)
  if (is_post(hm) && mg_match(hm->uri, mg_str("/signup"), NULL)) {
    handle_post_signup(c, hm);
    return;
  }

  // 4) /logout (exige sessão)
  if (is_post(hm) && mg_match(hm->uri, mg_str("/logout"), NULL)) {
    int uid = 0;
    if (!auth_require_user(c, hm, &uid)) return;
    handle_post_logout(c, hm);
    return;
  }

  // 5) /me (exige sessão)
  if (is_get(hm) && mg_match(hm->uri, mg_str("/me"), NULL)) {
    handle_get_me(c, hm);
    return;
  }

  // 6) Workouts e sets: exigem sessão
  if (mg_match(hm->uri, mg_str("/workouts"), NULL) ||
      mg_match(hm->uri, mg_str("/workouts/#"), NULL) ||
      mg_match(hm->uri, mg_str("/workouts/#/sets"), NULL) ||
      mg_match(hm->uri, mg_str("/workouts/#/sets/#"), NULL)) {

    int uid = 0;
    if (!auth_require_user(c, hm, &uid)) return;
  }

  // 7) Admin: exige sessão + role=admin
  if (mg_match(hm->uri, mg_str("/admin/#"), NULL)) {
    int uid = 0;
    if (!auth_require_admin(c, hm, &uid)) return;
  }

  // ======== ROUTES ========

  // -------- Exercises --------
  if (is_get(hm) && mg_match(hm->uri, mg_str("/exercises"), NULL)) {
    handle_get_exercises(c);

  } else if (is_get(hm) && mg_match(hm->uri, mg_str("/exercises/#"), NULL)) {
    handle_get_exercises_id(c, hm);

  } else if (is_post(hm) && mg_match(hm->uri, mg_str("/exercises"), NULL)) {
    int uid = 0;
    if (!auth_require_admin(c, hm, &uid)) return;
    handle_post_exercises(c, hm);

  } else if (is_put(hm) && mg_match(hm->uri, mg_str("/exercises/#"), NULL)) {
    int uid = 0;
    if (!auth_require_admin(c, hm, &uid)) return;
    handle_put_exercises(c, hm);

  } else if (is_delete(hm) && mg_match(hm->uri, mg_str("/exercises/#"), NULL)) {
    int uid = 0;
    if (!auth_require_admin(c, hm, &uid)) return;
    handle_delete_exercises(c, hm);

  // -------- Workouts (sets primeiro) --------
  } else if (is_post(hm) && mg_match(hm->uri, mg_str("/workouts/#/sets"), NULL)) {
    handle_post_workout_set(c, hm);

  } else if (is_put(hm) && mg_match(hm->uri, mg_str("/workouts/#/sets/#"), NULL)) {
    handle_put_workout_set(c, hm);

  } else if (is_delete(hm) && mg_match(hm->uri, mg_str("/workouts/#/sets/#"), NULL)) {
    handle_delete_workout_set(c, hm);

  // -------- Workouts --------
  } else if (is_get(hm) && mg_match(hm->uri, mg_str("/workouts"), NULL)) {
    handle_get_workouts(c, hm);

  } else if (is_get(hm) && mg_match(hm->uri, mg_str("/workouts/#"), NULL)) {
    handle_get_workouts_id(c, hm);

  } else if (is_post(hm) && mg_match(hm->uri, mg_str("/workouts"), NULL)) {
    handle_post_workouts(c, hm);

  } else if (is_put(hm) && mg_match(hm->uri, mg_str("/workouts/#"), NULL)) {
    handle_put_workouts(c, hm);

  } else if (is_delete(hm) && mg_match(hm->uri, mg_str("/workouts/#"), NULL)) {
    handle_delete_workouts(c, hm);

  // -------- Stats --------
  } else if (is_get(hm) && mg_match(hm->uri, mg_str("/stats/volume"), NULL)) {
    handle_get_stats_volume(c, hm);

  } else if (is_get(hm) && mg_match(hm->uri, mg_str("/stats/prs"), NULL)) {
    handle_get_stats_prs(c, hm);

  // -------- Admin --------
  } else if (is_post(hm) && mg_match(hm->uri, mg_str("/admin/users"), NULL)) {
    handle_post_admin_users(c, hm);

  } else if (is_get(hm) && mg_match(hm->uri, mg_str("/admin/users"), NULL)) {
    handle_get_admin_users(c);

  } else {
    handle_not_found(c);
  }
}
