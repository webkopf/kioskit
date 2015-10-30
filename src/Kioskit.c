/*                    Minimal Kiosk Browser

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version (see http://www.gnu.org/licenses/ ).

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 Copyright 2013 Ralph Glass (Minimal Web Browser base code)
 Copyright 2013-2015 Guenter Kreidl (Minimal Kiosk Browser)

 Version 1.6.9 using gtk+3 and webkitgtk-3.0
 */

#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static int window_count = 0;
static gboolean javascript = FALSE;
static gboolean kioskmode = FALSE;
static gboolean alternate = FALSE;
static gboolean cookies_allowed = FALSE;
static gboolean private_browsing = TRUE;
static gboolean no_window = FALSE;
static gboolean use_dlmanager = TRUE;
static gboolean open_executable = FALSE;
static gboolean maximize = TRUE;
static gboolean experimental = FALSE;
static gboolean full_zoom = FALSE;
static gboolean startpage = TRUE;
static gboolean useOMX = TRUE;
static gboolean page_cache = FALSE;
static gboolean commands_enabled = FALSE;
static gboolean show_uri_in_title = FALSE;
static gboolean no_autoplay = FALSE;
static gboolean smooth_scrolling = FALSE;
static gboolean multiple_windows = TRUE;
static int defaultw = 1920;
static int defaulth = 1080;
static SoupCookieJar* cookiejar;
static gchar* homedir;
static gchar* homepage;
static gchar* homecommand;
static gchar* homekbd;
static gchar* hometxt;
static gchar* kb_commands = "+-zbhrqfpoklgtjneduwxycsamvi:#=?!.";
static gchar* last_filepath = "";
static gchar* dl_command = "dlu";
static gchar* allowed_kb_commands = "+-zbhrqfpoklgtjneduwxycsamvi:#=?!.";
static gchar* search_str;
static gchar* command_flags;
static int hclen;
static int buttonmode = GTK_TOOLBAR_BOTH;
static int iconsize = GTK_ICON_SIZE_DND;
static int cache_model = WEBKIT_CACHE_MODEL_WEB_BROWSER;

static void loadUri(WebKitWebView* webView, gchar* uri) {
	webkit_web_view_stop_loading(webView);
	webkit_web_view_load_uri(webView, uri);
}

static void destroy(GtkWidget* widget, gpointer* data) {
	if (window_count < 2)
		gtk_main_quit();
	window_count--;
}

static void closeView(WebKitWebView* webView, GtkWidget* window) {
	gtk_widget_destroy(window);
}

static void goBack(GtkWidget* item, WebKitWebView* webView) {
	webkit_web_view_stop_loading(webView);
	webkit_web_view_go_back(webView);
}

static void goHome(GtkWidget* item, WebKitWebView* webView) {
	gchar *hp = g_object_get_data(G_OBJECT(webView), "homepage");
	loadUri(webView, hp);
}

static void stopLoading(GtkWidget* item, WebKitWebView* webView) {
	webkit_web_view_stop_loading(webView);
}

static void web_zoom_plus(GtkWidget* item, WebKitWebView* webView) {
	webkit_web_view_zoom_in(webView);
}

static void web_zoom_minus(GtkWidget* item, WebKitWebView* webView) {
	webkit_web_view_zoom_out(webView);
}

static void web_zoom_100(GtkWidget* item, WebKitWebView* webView) {
	gfloat zoom = webkit_web_view_get_zoom_level(webView);
	if (zoom != 1.0) {
		webkit_web_view_set_zoom_level(webView, 1.0);
	}
}

static gboolean downloadRequested(WebKitWebView* webView,
		WebKitDownload* download, GtkEntry* entry) {
	const char* uri = webkit_download_get_uri(download);
	int r = 0;
	r = fork();
	if (r == 0) {
		execl("/usr/local/bin/kwebhelper.py", "kwebhelper.py", dl_command, uri,
				NULL);
	}
	return FALSE;
}

static void downloadPage(GtkWidget* item, WebKitWebView* webView) {
	const gchar* uri = webkit_web_view_get_uri(webView);
	if (strncmp(uri, "file://", 7) != 0) {
		int r = 0;
		r = fork();
		if (r == 0) {
			execl("/usr/local/bin/kwebhelper.py", "kwebhelper.py", "dlp", uri,
					NULL);
		}
	}
}

static void searchText(WebKitWebView* webView, gchar* searchString) {
	webkit_web_view_unmark_text_matches(webView);
	webkit_web_view_search_text(webView, searchString, false, true, true);
	webkit_web_view_mark_text_matches(webView, searchString, false, 0);
	webkit_web_view_set_highlight_text_matches(webView, true);
}

static void setJbutton(GtkWidget* window, gboolean js) {
	GtkWidget *Jbutton = g_object_get_data(G_OBJECT(window), "jbutton");
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(Jbutton), js);
}

static void setCbutton(GtkWidget* window) {
	GtkWidget *Cbutton = g_object_get_data(G_OBJECT(window), "cbutton");
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(Cbutton),
			cookies_allowed);
}

static void setDbutton(GtkWidget* window) {
	GtkWidget *Dbutton = g_object_get_data(G_OBJECT(window), "dbutton");
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(Dbutton),
			use_dlmanager);
}

static void setZbutton(GtkWidget* window, gboolean fz) {
	GtkWidget *Zbutton = g_object_get_data(G_OBJECT(window), "zbutton");
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(Zbutton), fz);
}

static void setObutton(GtkWidget* window) {
	GtkWidget *Obutton = g_object_get_data(G_OBJECT(window), "obutton");
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(Obutton), useOMX);
}

static void setAbutton(GtkWidget* window) {
	GtkWidget *Abutton = g_object_get_data(G_OBJECT(window), "abutton");
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(Abutton),
			commands_enabled);
}

static gboolean setButtons(GtkWidget* window, GdkEventFocus* event,
		WebKitWebView* webView) {
	setCbutton(window);
	setDbutton(window);
	/*	setZbutton(window); */
	setObutton(window);
	setAbutton(window);
	return FALSE;
}

static void toggleJavascript(GtkWidget* item, WebKitWebView* webView) {
	gboolean js;
	WebKitWebSettings* settings = webkit_web_view_get_settings(
			WEBKIT_WEB_VIEW(webView));
	js = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(item));
	g_object_set(G_OBJECT(settings), "enable-scripts", js, NULL);
	webkit_web_view_reload(webView);
}

static void toggleDownload(GtkWidget* item, WebKitWebView* webView) {
	use_dlmanager = gtk_toggle_tool_button_get_active(
			GTK_TOGGLE_TOOL_BUTTON(item));
	if (use_dlmanager == TRUE) {
		dl_command = "dlu";
	} else {
		dl_command = "dlw";
	}
}

static void toggleOmx(GtkWidget* item, WebKitWebView* webView) {
	useOMX = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(item));
}

static void toggleCommand(GtkWidget* item, WebKitWebView* webView) {
	commands_enabled = gtk_toggle_tool_button_get_active(
			GTK_TOGGLE_TOOL_BUTTON(item));
}

static void toggleZoom(GtkWidget* item, WebKitWebView* webView) {
	gboolean fz;
	fz = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(item));
	webkit_web_view_set_full_content_zoom(webView, fz);
}

static void reload(GtkWidget* item, WebKitWebView* webView) {
	webkit_web_view_reload(webView);
}

static void togglefullscreen(GtkWidget* window, WebKitWebView* webView) {
	gchar* fs = g_object_get_data(G_OBJECT(window), "fullscreen");
	if (strcmp(fs, "u") == 0) {
		g_object_set_data(G_OBJECT(window), "fullscreen",
				g_strjoin(NULL, "f", NULL));
		gtk_window_fullscreen(GTK_WINDOW(window));
	} else {
		g_object_set_data(G_OBJECT(window), "fullscreen",
				g_strjoin(NULL, "u", NULL));
		gtk_window_unfullscreen(GTK_WINDOW(window));
	}
}

static void togglecontrols(GtkWidget* window) {
	GtkWidget* entry = g_object_get_data(G_OBJECT(window), "uriEntry");
	GtkWidget* toolbar = g_object_get_data(G_OBJECT(window), "toolbar");
	if (gtk_widget_get_visible(toolbar) == TRUE) {
		gtk_widget_hide(toolbar);
		gtk_widget_hide(entry);
	} else {
		gtk_widget_show(toolbar);
		gtk_widget_show(entry);
	}
}

static void toggleCookies(GtkWidget* item, WebKitWebView* webView) {
	cookies_allowed = gtk_toggle_tool_button_get_active(
			GTK_TOGGLE_TOOL_BUTTON(item));
	if (cookies_allowed == TRUE) {
		soup_cookie_jar_set_accept_policy(cookiejar,
				SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY);
	} else {
		soup_cookie_jar_set_accept_policy(cookiejar,
				SOUP_COOKIE_JAR_ACCEPT_NEVER);
	}
}

static void play_media(GtkWidget* item, WebKitWebView* webView) {
	const gchar* uri = webkit_web_view_get_uri(webView);
	int r = 0;
	r = fork();
	if (r == 0) {
		execl("/usr/local/bin/omxplayergui.py", "omxplayergui.py", "ytdl", uri,
				NULL);
	}
}

static void activateEntry(GtkWidget* entry, gpointer* gdata) {
	WebKitWebView* webView = g_object_get_data(G_OBJECT(entry), "webView");
	const gchar* entry_str = gtk_entry_get_text(GTK_ENTRY(entry));
	WebKitWebSettings* settings = webkit_web_view_get_settings(
			WEBKIT_WEB_VIEW(webView));
	gfloat current_zoom = webkit_web_view_get_zoom_level(webView);
	gboolean fz = webkit_web_view_get_full_content_zoom(webView);
	if (entry_str[0] != 0) {
		if (strncmp(entry_str, "?", 1) == 0) {
			gchar* s = g_strdup(entry_str);
			s++;
			gtk_entry_set_text(GTK_ENTRY(entry),
					g_strjoin(NULL, search_str, s, NULL));
			if ((startpage == TRUE) && (fz == TRUE) && (current_zoom != 1.0)) {
				fz = FALSE;
				webkit_web_view_set_full_content_zoom(webView, fz);
				GtkWidget* window = g_object_get_data(G_OBJECT(entry),
						"window");
				setZbutton(window, fz);
			}
		} else if (strncmp(entry_str, "=", 1) == 0) {
			gchar* s = g_strdup(entry_str);
			s++;
			searchText(webView, s);
			return;
		} else if (strncmp(entry_str, "#", 1) == 0) {
			int r = 0;
			r = fork();
			if (r == 0) {
				execl("/usr/local/bin/kwebhelper.py", "kwebhelper.py", "cmd",
						entry_str, NULL);
			}
			return;
		} else if (strncmp(entry_str, "!", 1) == 0) {
			gchar* s = g_strdup(entry_str);
			s++;
			if (strlen(s) > 1) {
				g_object_set(G_OBJECT(settings), "spell-checking-languages", s,
						NULL);
				gtk_entry_set_text(GTK_ENTRY(entry),
						g_strjoin(NULL,
								"Spell checking language has been set to: ", s,
								NULL));
			}
			return;
		}

		else if (strncmp(entry_str, "&", 1) == 0) {
			gchar* s = g_strdup(entry_str);
			s++;
			g_object_set(G_OBJECT(settings), "user-stylesheet-uri", s, NULL);
			gtk_entry_set_text(GTK_ENTRY(entry),
					g_strjoin(NULL, "User style sheet: ", s, NULL));
			return;
		} else if (strncmp(entry_str, "$", 1) == 0) {
			gchar* s = g_strdup(entry_str);
			s++;
			g_object_set(G_OBJECT(settings), "user-agent", s, NULL);
			const gchar* t = webkit_web_settings_get_user_agent(settings);
			gtk_entry_set_text(GTK_ENTRY(entry),
					g_strjoin(NULL, "User agent: ", t, NULL));
			return;
		}

		else if (strcmp(entry_str, ":c") == 0) {
			gtk_entry_set_text(GTK_ENTRY(entry),
					g_strjoin(NULL, "file://", homedir, "/kweb_about_c.html",
							NULL));
		} else if (strncmp(entry_str, ":", 1) == 0) {
			gchar* s = g_strdup(entry_str);
			s++;
			gchar* fname = g_strjoin(NULL, "/usr/local/share/kweb/kweb_about_",
					s, ".html", NULL);

			struct stat stbc;
			if (stat(fname, &stbc) == 0) {
				gtk_entry_set_text(GTK_ENTRY(entry),
						g_strjoin(NULL, "file://", fname, NULL));
			} else {
				gtk_entry_set_text(GTK_ENTRY(entry),
						g_strjoin(NULL,
								"file:///usr/local/share/kweb/kweb_about_e.html",
								NULL));
			}
		} else if (strstr(entry_str, "://") == NULL) {
			if (strncmp(entry_str, "/", 1) == 0) {
				gtk_entry_set_text(GTK_ENTRY(entry),
						g_strjoin(NULL, "file://", entry_str, NULL));
			} else {
				gtk_entry_set_text(GTK_ENTRY(entry),
						g_strjoin(NULL, "http://", entry_str, NULL));
			}
		}
		const gchar* uri = gtk_entry_get_text(GTK_ENTRY(entry));
		webkit_web_view_stop_loading(webView);
		webkit_web_view_load_uri(webView, uri);
		gtk_widget_grab_focus(GTK_WIDGET(webView));
	}
}

char *replace_str2(const char *str, const char *old, const char *new) {
	char *ret, *r;
	const char *p, *q;
	size_t oldlen = strlen(old);
	size_t count, retlen, newlen = strlen(new);
	int samesize = (oldlen == newlen);
	if (!samesize) {
		for (count = 0, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen)
			count++;
		retlen = p - str + strlen(p) + count * (newlen - oldlen);
	} else
		retlen = strlen(str);
	if ((ret = malloc(retlen + 1)) == NULL)
		return NULL;
	r = ret, p = str;
	while (1) {
		if (!samesize && !count--)
			break;
		if ((q = strstr(p, old)) == NULL)
			break;
		ptrdiff_t l = q - p;
		memcpy(r, p, l);
		r += l;
		memcpy(r, new, newlen);
		r += newlen;
		p = q + oldlen;
	}
	strcpy(r, p);
	return ret;
}

static void select_file(GtkWidget* item, GtkWidget* entry) {
	GtkWidget* window = g_object_get_data(G_OBJECT(entry), "window");
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new("Open File", GTK_WINDOW(window),
			GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL,
			"_Open", GTK_RESPONSE_ACCEPT,
			NULL);
	if (last_filepath != "") {
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), last_filepath);
	}
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar* selected_filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(dialog));
		last_filepath = g_strjoin(NULL, selected_filename, NULL);
		struct stat sb;
		if ((open_executable == TRUE)
				&& (stat(selected_filename, &sb) == 0 && sb.st_mode & S_IXUSR)) {
			gtk_entry_set_text(GTK_ENTRY(entry),
					g_strjoin(NULL, "#", selected_filename, NULL));
		} else {
			gtk_entry_set_text(GTK_ENTRY(entry),
					g_strjoin(NULL, "file://",
							replace_str2(selected_filename, "?", "%3F"), NULL));
		}
		activateEntry(entry, NULL);
		g_free(selected_filename);
	}
	gtk_widget_destroy(dialog);
}

static void new_browser(const gchar* uri) {
	int r = 0;
	r = fork();
	if (r == 0) {
		if (command_flags == NULL) {
			execl("/usr/bin/kweb3", "kweb3", uri, NULL);
		} else {
			execl("/usr/bin/kweb3", "kweb3", command_flags, uri, NULL);
		}
	}
}

static void kbdlink_command(GtkWidget* window, WebKitWebView* webView,
		GtkWidget* entry, int kbd) {
	int res;
	gchar *hp;
	gboolean js;
	gboolean fz = webkit_web_view_get_full_content_zoom(webView);
	WebKitWebSettings* settings = webkit_web_view_get_settings(
			WEBKIT_WEB_VIEW(webView));
	if (strchr(kb_commands, kbd) != NULL) {
		switch (kbd) {
		case '+':
			web_zoom_plus(window, webView);
			break;
		case '-':
			web_zoom_minus(window, webView);
			break;
		case 'i':
			gtk_entry_set_text(GTK_ENTRY(entry), "");
			gtk_widget_grab_focus(entry);
			break;
		case ':':
			gtk_widget_grab_focus(entry);
			gtk_entry_set_text(GTK_ENTRY(entry), ":");
			gtk_editable_set_position(GTK_EDITABLE(entry), -1);
			break;
		case '#':
			gtk_widget_grab_focus(entry);
			gtk_entry_set_text(GTK_ENTRY(entry), "#");
			gtk_editable_set_position(GTK_EDITABLE(entry), -1);
			break;
		case '!':
			gtk_widget_grab_focus(entry);
			gtk_entry_set_text(GTK_ENTRY(entry), "!");
			gtk_editable_set_position(GTK_EDITABLE(entry), -1);
			break;
		case '?':
			gtk_widget_grab_focus(entry);
			gtk_entry_set_text(GTK_ENTRY(entry), "?");
			gtk_editable_set_position(GTK_EDITABLE(entry), -1);
			break;
		case '=':
			gtk_widget_grab_focus(entry);
			gtk_entry_set_text(GTK_ENTRY(entry), "=");
			gtk_editable_set_position(GTK_EDITABLE(entry), -1);
			break;
		case 'z':
			web_zoom_100(window, webView);
			break;
		case 'j':
			g_object_get(G_OBJECT(settings), "enable-scripts", &js, NULL);
			if (js == FALSE) {
				js = TRUE;
				g_object_set(G_OBJECT(settings), "enable-scripts", js, NULL);
				webkit_web_view_reload(webView);
				setJbutton(window, js);
			}
			break;
		case 'n':
			g_object_get(G_OBJECT(settings), "enable-scripts", &js, NULL);
			if (js == TRUE) {
				js = FALSE;
				g_object_set(G_OBJECT(settings), "enable-scripts", js, NULL);
				webkit_web_view_reload(webView);
				setJbutton(window, js);
			}
			break;
		case 'e':
			if (cookies_allowed == FALSE) {
				cookies_allowed = TRUE;
				soup_cookie_jar_set_accept_policy(cookiejar,
						SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY);
				setCbutton(window);
			}
			break;
		case 'd':
			if (cookies_allowed == TRUE) {
				cookies_allowed = FALSE;
				soup_cookie_jar_set_accept_policy(cookiejar,
						SOUP_COOKIE_JAR_ACCEPT_NEVER);
				setCbutton(window);
			}
			break;
		case 'w':
			if (use_dlmanager == TRUE) {
				use_dlmanager = FALSE;
				dl_command = "dlw";
				setDbutton(window);
			}
			break;
		case 'u':
			if (use_dlmanager == FALSE) {
				use_dlmanager = TRUE;
				dl_command = "dlu";
				setDbutton(window);
			}
			break;
		case 'y':
			if (useOMX == TRUE) {
				useOMX = FALSE;
				setObutton(window);
			}
			break;
		case 'x':
			if (useOMX == FALSE) {
				useOMX = TRUE;
				setObutton(window);
			}
			break;
		case 't':
			if (fz == TRUE) {
				fz = FALSE;
				webkit_web_view_set_full_content_zoom(webView, fz);
				setZbutton(window, fz);
			}
			break;
		case 'g':
			if (fz == FALSE) {
				fz = TRUE;
				webkit_web_view_set_full_content_zoom(webView, fz);
				setZbutton(window, fz);
			}
			break;
		case 'v':
			if (commands_enabled == TRUE) {
				commands_enabled = FALSE;
				setAbutton(window);
			}
			break;
		case 'a':
			if (commands_enabled == FALSE) {
				commands_enabled = TRUE;
				setAbutton(window);
			}
			break;
		case 'h':
			goHome(window, webView);
			break;
		case 'r':
			reload(window, webView);
			break;
		case 'b':
			goBack(window, webView);
			break;
		case 'p':
			play_media(window, webView);
			break;
		case 'q':
			closeView(webView, window);
			break;
		case 'f':
			togglefullscreen(window, webView);
			break;
		case 'o':
			select_file(window, entry);
			break;
		case 'k':
			togglecontrols(window);
			break;
		case 'c':
			downloadPage(window, webView);
			break;
		case 's':
			stopLoading(window, webView);
			break;
		case 'm':
			loadUri(webView, "file:///usr/local/share/kweb/kweb_about_m.html");
			break;
		case 'l':
			hp = g_strjoin(NULL, webkit_web_view_get_uri(webView), NULL);
			g_object_set_data(G_OBJECT(webView), "homepage", hp);
			break;
		case '0':
			loadUri(webView,
					g_strjoin(NULL, "file://", homedir, "/kwebcontrol/kweb",
							"0", ".html", NULL));
			break;
		case '1':
			loadUri(webView,
					g_strjoin(NULL, "file://", homedir, "/kwebcontrol/kweb",
							"1", ".html", NULL));
			break;
		case '2':
			loadUri(webView,
					g_strjoin(NULL, "file://", homedir, "/kwebcontrol/kweb",
							"2", ".html", NULL));
			break;
		case '3':
			loadUri(webView,
					g_strjoin(NULL, "file://", homedir, "/kwebcontrol/kweb",
							"3", ".html", NULL));
			break;
		case '4':
			loadUri(webView,
					g_strjoin(NULL, "file://", homedir, "/kwebcontrol/kweb",
							"4", ".html", NULL));
			break;
		case '5':
			loadUri(webView,
					g_strjoin(NULL, "file://", homedir, "/kwebcontrol/kweb",
							"5", ".html", NULL));
			break;
		case '6':
			loadUri(webView,
					g_strjoin(NULL, "file://", homedir, "/kwebcontrol/kweb",
							"6", ".html", NULL));
			break;
		case '7':
			loadUri(webView,
					g_strjoin(NULL, "file://", homedir, "/kwebcontrol/kweb",
							"7", ".html", NULL));
			break;
		case '8':
			loadUri(webView,
					g_strjoin(NULL, "file://", homedir, "/kwebcontrol/kweb",
							"8", ".html", NULL));
			break;
		case '9':
			loadUri(webView,
					g_strjoin(NULL, "file://", homedir, "/kwebcontrol/kweb",
							"9", ".html", NULL));
			break;
		case '.':
			if ((kioskmode == FALSE)
					|| ((kioskmode == TRUE) && (multiple_windows == TRUE))) {
				new_browser(webkit_web_view_get_uri(webView));
				if (window_count > 1) {
					closeView(webView, window);
				}
			}
			break;
		}
	}
}

static gboolean web_key_pressed(GtkWidget* window, GdkEventKey* event,
		WebKitWebView* webView) {

	g_print("hallo");

	GtkWidget* entry = g_object_get_data(G_OBJECT(window), "uriEntry");
	gboolean alt = TRUE;
	int res;
	if (alternate == TRUE) {
		if (event->state & GDK_MOD1_MASK) {
			alt = TRUE;
		} else {
			alt = FALSE;
		}
	}
	if ((event->type == GDK_KEY_PRESS) && (alt == TRUE)
			&& (strchr(kb_commands, (event->keyval)) != NULL)) {
		kbdlink_command(window, webView, entry, event->keyval);
	}
	return FALSE;
}

static gboolean navigationPolicyDecision(WebKitWebView* webView,
		WebKitWebFrame* frame, WebKitNetworkRequest* request,
		WebKitWebNavigationAction* action, WebKitWebPolicyDecision* decision,
		GtkEntry* entry) {
	const char* uri = webkit_network_request_get_uri(request);
	const char* siteuri;
	WebKitWebFrame* parentframe = webkit_web_frame_get_parent(frame);
	if ((strncmp(uri, homecommand, hclen - 4) == 0)
			|| (strncmp(uri, "file://~/", 9) == 0)) {
		if (parentframe == NULL) {
			siteuri = webkit_web_view_get_uri(webView);
		} else {
			siteuri = webkit_web_frame_get_uri(parentframe);
		}
		if (siteuri != NULL) {
			if (strncmp(uri, homecommand, hclen) == 0) {
				if (strncmp(siteuri, homecommand, hclen - 18) == 0) {
					if (commands_enabled == TRUE) {
						int r = 0;
						r = fork();
						if (r == 0) {
							execl("/usr/local/bin/kwebhelper.py",
									"kwebhelper.py", "cmd", uri, NULL);
						}
					} else {
						webkit_web_frame_load_alternate_string(frame,
								"You must activate command execution in the toolbar or with ALT+a",
								"file:///", "homepage.html");
					}
				}
				webkit_web_policy_decision_ignore(decision);
				return TRUE;
			}
			if (strncmp(uri, "file://~/", 9) == 0) {
				const char* newuri = g_strjoin(NULL, "file://", homedir,
						uri + 8, NULL);
				if (strncmp(siteuri, "file://", 7) == 0) {
					webkit_web_frame_load_uri(frame, newuri);
					webkit_web_policy_decision_use(decision);
					return TRUE;
				}
				webkit_web_policy_decision_ignore(decision);
				return TRUE;
			}
			if ((strncmp(uri, homekbd, hclen) == 0)
					&& (strlen(uri) == hclen + 1)) {
				if (strncmp(siteuri, homecommand, hclen - 18) == 0) {
					if (commands_enabled == TRUE) {

						int kbdc = uri[26];
						GtkWidget *window = g_object_get_data(G_OBJECT(entry),
								"window");
						kbdlink_command(window, webView, GTK_WIDGET(entry),
								kbdc);
					}
				} else {
					webkit_web_frame_load_alternate_string(frame,
							"You must enable command links in the toolbar or with ALT+a",
							"file:///", "homepage.html");
				}
				webkit_web_policy_decision_ignore(decision);
				return TRUE;
			}

			if (strncmp(uri, hometxt, hclen) == 0) {
				if (strncmp(siteuri, homecommand, hclen - 18) == 0) {
					if (commands_enabled == TRUE) {
						gtk_entry_set_text(GTK_ENTRY(entry),
								replace_str2(uri + hclen, "%20", " "));
						activateEntry(GTK_WIDGET(entry), NULL);
					}
				} else {
					webkit_web_frame_load_alternate_string(frame,
							"You must enable command links in the toolbar or with ALT+a",
							"file:///", "homepage.html");
				}
				webkit_web_policy_decision_ignore(decision);
				return TRUE;
			}

		} else {
			if ((strncmp(uri, homekbd, hclen) == 0)
					|| (strncmp(uri, hometxt, hclen) == 0)
					|| (strncmp(uri, homecommand, hclen) == 0)
					|| (strncmp(uri, "file://~/", 9) == 0)) {
				webkit_web_policy_decision_ignore(decision);
				return TRUE;
			}
		}
	}

	if ((strncmp(uri, "rtmp://", 7) == 0) || (strncmp(uri, "rtp://", 6) == 0)
			|| (strncmp(uri, "rtsp://", 7) == 0)
			|| (strncmp(uri, "mmsh://", 7) == 0)) {
		int r = 0;
		r = fork();
		if (r == 0) {
			execl("/usr/local/bin/omxplayergui.py", "omxplayergui.py", "av",
					uri, NULL);
		}
		webkit_web_policy_decision_ignore(decision);
		return TRUE;
	}

	if (parentframe == NULL) {
		gtk_entry_set_text(entry, uri);
		if (show_uri_in_title == TRUE) {
			GtkWindow *window = g_object_get_data(G_OBJECT(entry), "window");
			gtk_window_set_title(window, uri);
		}
	}
	return FALSE;
}

static gboolean mimePolicyDecision(WebKitWebView* webView,
		WebKitWebFrame* frame, WebKitNetworkRequest* request, gchar* mimetype,
		WebKitWebPolicyDecision* policy_decision, gpointer* user_data) {
	const char* uri = webkit_network_request_get_uri(request);
	if (((strncmp(mimetype, "video/", 6) == 0)
			|| (strncmp(mimetype, "audio/", 6) == 0)) && (useOMX == TRUE)) {
		int r = 0;
		r = fork();
		if (r == 0) {
			execl("/usr/local/bin/omxplayergui.py", "omxplayergui.py", "av",
					uri, mimetype, NULL);
		}
		webkit_web_policy_decision_ignore(policy_decision);
		return TRUE;
	} else if (strncmp(mimetype, "application/pdf", 15) == 0) {
		int r = 0;
		r = fork();
		if (r == 0) {
			execl("/usr/local/bin/kwebhelper.py", "kwebhelper.py", "pdf", uri,
					NULL);
		}
		webkit_web_policy_decision_ignore(policy_decision);
		return TRUE;
	}
	return FALSE;
}

static char* check_starturi(gchar* ouri) {
	gchar* rstr;
	if (ouri == NULL) {
		rstr = homepage;
	} else {
		if ((strncmp(ouri, homecommand, hclen) == 0)
				|| (strncmp(ouri, hometxt, hclen) == 0)
				|| (strncmp(ouri, homekbd, hclen) == 0)
				|| (strncmp(ouri, "file://~/", 9) == 0)) {
			rstr = homepage;
		} else {
			rstr = ouri;
		}
	}
	return rstr;
}

static gboolean check_new_window(WebKitWebView *web_view, WebKitWebFrame *frame,
		WebKitNetworkRequest *request,
		WebKitWebNavigationAction *navigation_action,
		WebKitWebPolicyDecision *policy_decision, gpointer user_data) {
	gint newbrowser = webkit_web_navigation_action_get_modifier_state(
			navigation_action);
	if (newbrowser != 1) {
		return FALSE;
	} else {
		const gchar* uri = webkit_network_request_get_uri(request);
		new_browser(uri);
		webkit_web_policy_decision_ignore(policy_decision);
		return TRUE;
	}
}

static void on_document_loaded(WebKitWebView* web_view) {
	g_print("dokument geladen");

	WebKitDOMDocument* domDocument = webkit_web_view_get_dom_document(web_view);

	WebKitDOMHTMLElement* body = webkit_dom_document_get_body(domDocument);

	webkit_dom_element_set_attribute(body, "style", "background: pink", NULL);

	g_print("blubb");

}

static WebKitWebView* createWebView(WebKitWebView* parentWebView,
		WebKitWebFrame* frame, gchar* arg) {
	gboolean js = javascript;
	gboolean fz = full_zoom;
	gfloat current_zoom = 1.0;
	WebKitWebSettings* settings;
	WebKitWebSettings* psettings;
	const gchar* nuri = check_starturi(arg);
	window_count++;
	GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* scrolledWindow = gtk_scrolled_window_new(NULL, NULL);

	GtkWidget* uriEntry = gtk_entry_new();
	gtk_widget_set_tooltip_text(uriEntry,
			"Enter URL (i)   ?web search (?)   =page search (=)   #command line (#)   :command (:)   !language (!)   &user style URI   $user-agent");
	GtkWidget* toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), buttonmode);
	if (iconsize == GTK_ICON_SIZE_SMALL_TOOLBAR) {
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), iconsize);
	}

	GtkToolItem* item;
	WebKitWebView* webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
	settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webView));
	g_object_set(G_OBJECT(settings), "enable-private-browsing",
			private_browsing, NULL);
	g_object_set(G_OBJECT(settings), "enable-file-access-from-file-uris", TRUE,
			NULL);
	g_object_set(G_OBJECT(settings), "enable-universal-access-from-file-uris",
			TRUE, NULL);
	g_object_set(G_OBJECT(settings), "enable-spatial-navigation", TRUE, NULL);
	g_object_set(G_OBJECT(settings), "default-encoding", "utf-8", NULL);
	g_object_set(G_OBJECT(settings), "enable-page-cache", page_cache, NULL);
	g_object_set(G_OBJECT(settings), "enable-plugins", TRUE, NULL);
	g_object_set(G_OBJECT(settings), "enable-site-specific-quirks", TRUE, NULL);
	g_object_set(G_OBJECT(settings), "enable-accelerated-compositing", TRUE,
			NULL);
	g_object_set(G_OBJECT(settings), "enable-webgl", FALSE, NULL);
	g_object_set(G_OBJECT(settings), "enable-webaudio", experimental, NULL);
	g_object_set(G_OBJECT(settings), "enable-media-stream", experimental, NULL);
	g_object_set(G_OBJECT(settings), "enable-mediasource", experimental, NULL);
	g_object_set(G_OBJECT(settings), "enable-smooth-scrolling",
			smooth_scrolling, NULL);
	g_object_set(G_OBJECT(settings), "enable-spell-checking", TRUE, NULL);
	g_object_set(G_OBJECT(settings), "media-playback-requires-user-gesture",
			no_autoplay, NULL);

	if (parentWebView != NULL) {
		psettings = webkit_web_view_get_settings(
				WEBKIT_WEB_VIEW(parentWebView));
		g_object_set(G_OBJECT(settings), "spell-checking-languages",
				g_object_get_data(G_OBJECT(psettings),
						"spell-checking-languages"), NULL);
		g_object_set(G_OBJECT(settings), "user-stylesheet-uri",
				g_object_get_data(G_OBJECT(psettings), "user-stylesheet-uri"),
				NULL);
		g_object_set(G_OBJECT(settings), "user-agent",
				webkit_web_settings_get_user_agent(psettings), NULL);
		g_object_get(G_OBJECT(psettings), "enable-scripts", &js, NULL);
		fz = webkit_web_view_get_full_content_zoom(parentWebView);
		current_zoom = webkit_web_view_get_zoom_level(parentWebView);
	}

	g_object_set(G_OBJECT(settings), "enable-scripts", js, NULL);
	webkit_web_view_set_full_content_zoom(webView, fz);
	webkit_web_view_set_zoom_level(webView, current_zoom);

	gtk_entry_set_text(GTK_ENTRY(uriEntry), nuri);

	g_signal_connect(webView, "document-load-finished",
			G_CALLBACK(on_document_loaded), webView);

	item = gtk_tool_button_new(NULL, "Open");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "document-open");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item), " Open file (o)");
	g_signal_connect(G_OBJECT( item ), "clicked", G_CALLBACK( select_file ),
			uriEntry);

	item = gtk_tool_button_new(NULL, "Back");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "go-previous");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item), " Back to last page (b)");
	g_signal_connect(G_OBJECT( item ), "clicked", G_CALLBACK( goBack ),
			webView);

	item = gtk_tool_button_new(NULL, "Stop");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "process-stop");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item), " Stop loading (s)");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(stopLoading),
			webView);

	item = gtk_tool_button_new(NULL, "Reload");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "view-refresh");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item), " Reload page (r)");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(reload), webView);

	item = gtk_tool_button_new(NULL, "Home");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "go-home");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item), " Open your homepage (h)");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(goHome), webView);

	item = gtk_tool_button_new(NULL, "Play");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item),
			"media-playback-start");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item),
			" Play video from website (p)");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(play_media),
			webView);

	item = gtk_tool_button_new(NULL, "Zoom in");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "zoom-in");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item), " Zoom in by 10% (+)");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(web_zoom_plus),
			webView);

	item = gtk_tool_button_new(NULL, "Zoom out");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "zoom-out");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item), " Zoom out by 10% (-)");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(web_zoom_minus),
			webView);

	item = gtk_tool_button_new(NULL, "Zoom 100%");
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "zoom-original");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item), " Reset zoom to 100% (z)");
	g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(web_zoom_100),
			webView);

	item = gtk_separator_tool_item_new();
	gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(item), TRUE);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);

	item = gtk_toggle_tool_button_new();
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "zoom-fit-best");
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), "Full Zoom");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(item), fz);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item),
			" Global, full zoom or text zoom only (g,t)");
	g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(toggleZoom),
			webView);
	g_object_set_data(G_OBJECT(window), "zbutton",
			GTK_TOGGLE_TOOL_BUTTON(item));

	item = gtk_toggle_tool_button_new();
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "text-x-script");
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), "Javascript");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(item), js);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item),
			" Javascript or No Javascript (j,n)");
	g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(toggleJavascript),
			webView);
	g_object_set_data(G_OBJECT(window), "jbutton",
			GTK_TOGGLE_TOOL_BUTTON(item));

	item = gtk_toggle_tool_button_new();
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "security-medium");
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), "Cookies");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(item),
			cookies_allowed);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item),
			" Enable/disable cookies (e,d)");
	g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(toggleCookies),
			webView);
	g_object_set_data(G_OBJECT(window), "cbutton",
			GTK_TOGGLE_TOOL_BUTTON(item));

	item = gtk_toggle_tool_button_new();
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "emblem-downloads");
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), "DL-Manager");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(item),
			use_dlmanager);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item),
			"uGet or wget for downloads (u,w)");
	g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(toggleDownload),
			webView);
	g_object_set_data(G_OBJECT(window), "dbutton",
			GTK_TOGGLE_TOOL_BUTTON(item));

	item = gtk_toggle_tool_button_new();
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item),
			"applications-multimedia");
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), "Omxplayer");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(item), useOMX);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item),
			" Use omXplayer for media (x,y)");
	g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(toggleOmx), webView);
	g_object_set_data(G_OBJECT(window), "obutton",
			GTK_TOGGLE_TOOL_BUTTON(item));

	item = gtk_toggle_tool_button_new();
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), "system-run");
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), "Commands");
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(item),
			commands_enabled);
	gtk_widget_set_tooltip_text(GTK_WIDGET(item),
			" Activate/veto command execution (a,v)");
	g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(toggleCommand),
			webView);
	g_object_set_data(G_OBJECT(window), "abutton",
			GTK_TOGGLE_TOOL_BUTTON(item));

	g_object_set_data(G_OBJECT(uriEntry), "webView", webView);
	g_object_set_data(G_OBJECT(uriEntry), "window", window);
	g_object_set_data(G_OBJECT(window), "uriEntry", uriEntry);
	g_object_set_data(G_OBJECT(window), "webView", webView);
	g_object_set_data(G_OBJECT(window), "toolbar", toolbar);

	g_object_set_data(G_OBJECT(webView), "homepage",
			g_strjoin(NULL, homepage, NULL));

	gtk_window_set_default_size(GTK_WINDOW(window), defaultw, defaulth);

	if (maximize == TRUE) {
		gtk_window_maximize(GTK_WINDOW(window));
	}
	if ((kioskmode == TRUE) && (window_count < 2) && (no_window == FALSE)) {
		g_object_set_data(G_OBJECT(window), "fullscreen",
				g_strjoin(NULL, "f", NULL));
		gtk_window_fullscreen(GTK_WINDOW(window));
	} else {
		g_object_set_data(G_OBJECT(window), "fullscreen",
				g_strjoin(NULL, "u", NULL));
	}

	gtk_container_add(GTK_CONTAINER(scrolledWindow), GTK_WIDGET(webView));
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), uriEntry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledWindow, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);
	g_signal_connect(webView, "close-web-view", G_CALLBACK(closeView), window);
	g_signal_connect(uriEntry, "activate", G_CALLBACK(activateEntry), NULL);
	g_signal_connect(window, "focus-in-event", G_CALLBACK(setButtons), webView);
	g_signal_connect(window, "visibility-notify-event", G_CALLBACK(setButtons),
			webView);

	if ((kioskmode == FALSE)
			|| ((kioskmode == TRUE) && (multiple_windows == TRUE))) {
		g_signal_connect(webView, "create-web-view", G_CALLBACK(createWebView),
				NULL);
		g_signal_connect(webView, "new-window-policy-decision-requested",
				G_CALLBACK(check_new_window), NULL);
	}
	g_signal_connect(webView, "download-requested",
			G_CALLBACK(downloadRequested), uriEntry);
	g_signal_connect(webView, "navigation-policy-decision-requested",
			G_CALLBACK(navigationPolicyDecision), uriEntry);
	g_signal_connect(webView, "mime-type-policy-decision-requested",
			G_CALLBACK(mimePolicyDecision), NULL);
	if (kb_commands != "") {
		gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);
		g_signal_connect(window, "key-press-event", G_CALLBACK(web_key_pressed),
				webView);
	}
	gtk_widget_activate(uriEntry);
	gtk_widget_grab_focus(GTK_WIDGET(webView));
	gtk_widget_show_all(window);
	if ((kioskmode == TRUE) && (window_count < 2)) {
		gtk_widget_hide(GTK_WIDGET(toolbar));
		gtk_widget_hide(GTK_WIDGET(uriEntry));
	}
	return webView;
}

void signal_catcher(int signal) {
	int status;
	int chpid = waitpid(-1, &status, WNOHANG);
}

void set_cache_model() {
	if (cache_model != WEBKIT_CACHE_MODEL_WEB_BROWSER) {
		webkit_set_cache_model(cache_model);
	}
}

int main(int argc, char* argv[]) {
	homedir = getenv("HOME");
	gboolean dummy;
	dummy = g_setenv("WEBKIT_DISABLE_TBS", "1", FALSE);
	const char* cookie_file_name = g_strjoin(NULL, homedir, "/.web_cookie_jar",
			NULL);
	gtk_init(&argc, &argv);
	SoupSession* session = webkit_get_default_session();
	SoupSessionFeature* feature;

	cookiejar = soup_cookie_jar_text_new(cookie_file_name, FALSE);
	feature = (SoupSessionFeature*) (cookiejar);
	soup_session_add_feature(session, feature);
	soup_cookie_jar_set_accept_policy(cookiejar, SOUP_COOKIE_JAR_ACCEPT_NEVER);
	homepage = g_strjoin(NULL, "file://", homedir, "/homepage.html", NULL);
	search_str = "https://startpage.com/do/search?q=";
	struct stat stb;
	if (stat(g_strjoin(NULL, homedir, "/homepage.html", NULL), &stb) != 0) {
		homepage = g_strjoin(NULL, "file://",
				"/usr/local/share/kweb/kweb_about_m.html", NULL);
	}
	homecommand = "file:///homepage.html?cmd=";
	homekbd = "file:///homepage.html?kbd=";
	hometxt = "file:///homepage.html?txt=";
	hclen = strlen(homecommand);
	bool from_configfile = FALSE;
	char *arg1 = argv[1];
	char *arg2 = argv[2];
	if (arg1 == NULL) {
		char * configfile = g_strjoin(NULL, homedir, "/.kweb.conf", NULL);
		if (stat(configfile, &stb) == 0) {
			FILE *ifp;
			char buf1[100];
			char buf2[255];
			char *ReturnCode;
			ifp = fopen(configfile, "r");
			ReturnCode = fgets((char *) buf1, 100, ifp);
			ReturnCode = fgets((char *) buf2, 255, ifp);
			int dummy = fclose(ifp);
			from_configfile = TRUE;
			if (strlen(buf1) < 3) {
				arg1 = NULL;
			} else {
				arg1 = g_strjoin(NULL, buf1, NULL);
				if (arg1[strlen(arg1) - 1] == '\n') {
					arg1[strlen(arg1) - 1] = '\0';
				}
			}
			if (strlen(buf2) < 7) {
				arg2 = NULL;
			} else {
				if (arg1 == NULL) {
					arg1 = g_strjoin(NULL, buf2, NULL);
					if (arg1[strlen(arg1) - 1] == '\n') {
						arg1[strlen(arg1) - 1] = '\0';
					}
				} else {
					arg2 = g_strjoin(NULL, buf2, NULL);
					if (arg2[strlen(arg2) - 1] == '\n') {
						arg2[strlen(arg2) - 1] = '\0';
					}
				}
			}
		}
	}
	if (arg1 != NULL) {
		if (strncmp(arg1, "-", 1) == 0) {
			command_flags = g_strjoin(NULL, arg1, NULL);
			int l = strlen(arg1);
			int len;
			kb_commands = "";
			int i;
			char res[] = "";
			for (i = 1; i < l; i++) {
				switch (arg1[i]) {
				case 'K':
					kioskmode = TRUE;
					break;
				case 'A':
					alternate = TRUE;
					break;
				case 'J':
					javascript = TRUE;
					break;
				case 'E':
					cookies_allowed = TRUE;
					soup_cookie_jar_set_accept_policy(cookiejar,
							SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY);
					break;
				case 'I':
					buttonmode = GTK_TOOLBAR_ICONS;
					break;
				case 'T':
					buttonmode = GTK_TOOLBAR_TEXT;
					break;
				case 'S':
					iconsize = GTK_ICON_SIZE_SMALL_TOOLBAR;
					break;
				case 'H':
					if (arg2 != NULL) {
						homepage = arg2;
					}
					break;
				case 'L':
					if (arg2 != NULL) {
						if ((strncmp(arg2, "http://localhost:", 17) == 0)
								|| (strncmp(arg2, "http://localhost/", 17) == 0)) {
							homecommand = g_strjoin(NULL, arg2,
									"homepage.html?cmd=", NULL);
							homekbd = g_strjoin(NULL, arg2,
									"homepage.html?kbd=", NULL);
							hometxt = g_strjoin(NULL, arg2,
									"homepage.html?txt=", NULL);
							hclen = strlen(homecommand);
							homepage = arg2;
						}
					}
					break;
				case 'P':
					private_browsing = FALSE;
					break;
				case 'N':
					no_window = TRUE;
					break;
				case 'M':
					maximize = FALSE;
					break;
				case 'W':
					use_dlmanager = FALSE;
					dl_command = "dlw";
					break;
				case 'X':
					open_executable = TRUE;
					break;
				case 'Z':
					full_zoom = TRUE;
					break;
				case 'G':
					startpage = FALSE;
					search_str = "https://www.google.com/search?as_q=";
					break;
				case 'F':
					experimental = TRUE;
					break;
				case 'Y':
					useOMX = FALSE;
					break;
				case 'B':
					page_cache = TRUE;
					break;
				case 'C':
					if (from_configfile == FALSE) {
						commands_enabled = TRUE;
					}
					break;
				case 'V':
					show_uri_in_title = TRUE;
					break;
				case 'U':
					cache_model = WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER;
					break;
				case 'D':
					cache_model = WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER;
					break;
				case 'Q':
					smooth_scrolling = TRUE;
					break;
				case 'R':
					multiple_windows = FALSE;
					break;
				case '0':
					defaultw = 640;
					defaulth = 480;
					break;
				case '1':
					defaultw = 768;
					defaulth = 576;
					break;
				case '2':
					defaultw = 800;
					defaulth = 600;
					break;
				case '3':
					defaultw = 1024;
					defaulth = 768;
					break;
				case '4':
					defaultw = 1280;
					defaulth = 1024;
					break;
				case '5':
					defaultw = 1280;
					defaulth = 720;
					break;
				case '6':
					defaultw = 1366;
					defaulth = 768;
					break;
				case '7':
					defaultw = 1600;
					defaulth = 900;
					break;
				case '8':
					defaultw = 1600;
					defaulth = 1050;
					break;
				case '9':
					defaultw = 1920;
					defaulth = 1200;
					break;
				default:
					if (strchr(allowed_kb_commands, arg1[i]) != NULL) {
						if (strchr(res, arg1[i]) == NULL) {
							len = strlen(res);
							res[len + 1] = res[len];
							res[len] = arg1[i];
						}
					}
				}
			}

			if (stat(g_strjoin(NULL, homedir, "/kwebcontrol", NULL), &stb)
					== 0) {
				kb_commands = g_strjoin(NULL, res, "0123456789", NULL);
			} else {
				kb_commands = g_strjoin(NULL, res, NULL);
			}

			if (arg2 != NULL) {
				set_cache_model();
				createWebView( NULL, NULL, arg2);
			} else {
				set_cache_model();
				createWebView( NULL, NULL, NULL);
			}
		} else {
			set_cache_model();
			createWebView( NULL, NULL, arg1);
		}
	} else {
		set_cache_model();
		createWebView( NULL, NULL, NULL);
	}
	if (kioskmode == FALSE) {
		alternate = TRUE;
	}
	signal(SIGCHLD, signal_catcher);
	gtk_main();
	return 0;
}
