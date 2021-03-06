/* $Id$ */
/*-
 * Copyright (c) 2003-2007 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef XFCE_DISABLE_DEPRECATED
#undef XFCE_DISABLE_DEPRECATED
#endif

#include <X11/Xlib.h>

#include <gdk-pixbuf/gdk-pixdata.h>
#include <gmodule.h>

#include <libxfce4ui/libxfce4ui.h>

#include <libxfsm/xfsm-splash-engine.h>

#include <engines/simple/fallback.h>
#include <engines/simple/preview.h>


#define BORDER          2

#define DEFAULT_FONT    "Sans Bold 10"
#define DEFAULT_BGCOLOR "Black"
#define DEFAULT_FGCOLOR "White"


typedef struct _Simple Simple;
struct _Simple
{
  gboolean         dialog_active;
  GdkWindow       *window;
  PangoLayout     *layout;
  GdkPixbuf       *logo;
  GdkRectangle     area;
  GdkRectangle     textbox;
  GdkRGBA          bgcolor;
  GdkRGBA          fgcolor;
};


G_MODULE_EXPORT void config_init (XfsmSplashConfig *config);
G_MODULE_EXPORT void engine_init (XfsmSplashEngine *engine);


static GdkFilterReturn
simple_filter (GdkXEvent *xevent, GdkEvent *event, gpointer user_data)
{
  Simple *simple = (Simple *) user_data;
  XVisibilityEvent *xvisev = (XVisibilityEvent *) xevent;

  switch (xvisev->type)
    {
    case VisibilityNotify:
      if (!simple->dialog_active)
        {
          gdk_window_raise (simple->window);
          return GDK_FILTER_REMOVE;
        }
      break;
    }

  return GDK_FILTER_CONTINUE;
}


static void
simple_setup (XfsmSplashEngine *engine,
              XfsmSplashRc     *rc)
{
  PangoFontDescription *description;
  PangoFontMetrics     *metrics;
  PangoContext         *context;
  GdkWindowAttr         attr;
  GdkRectangle          geo;
  gchar                *color;
  gchar                *font;
  gchar                *path;
  GdkWindow            *root;
  GdkCursor            *cursor;
  Simple               *simple;
  gint                  logo_width;
  gint                  logo_height;
  gint                  text_height;
  cairo_t              *cr;

  simple = (Simple *) engine->user_data;

  /* load settings */
  color = xfsm_splash_rc_read_entry (rc, "BgColor", DEFAULT_BGCOLOR);
  gdk_rgba_parse (&simple->bgcolor, color);
  g_free (color);

  color = xfsm_splash_rc_read_entry (rc, "FgColor", DEFAULT_FGCOLOR);
  gdk_rgba_parse (&simple->fgcolor, color);
  g_free (color);

  font = xfsm_splash_rc_read_entry (rc, "Font", DEFAULT_FONT);
  path = xfsm_splash_rc_read_entry (rc, "Image", NULL);

  root = gdk_screen_get_root_window (engine->primary_screen);
  gdk_screen_get_monitor_geometry (engine->primary_screen,
                                   engine->primary_monitor,
                                   &geo);

  if (path != NULL && g_file_test (path, G_FILE_TEST_IS_REGULAR))
    simple->logo = gdk_pixbuf_new_from_file (path, NULL);
  if (simple->logo == NULL)
  {
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    /* TODO: use GResource or load it as a normal pixbuf */
    simple->logo = gdk_pixbuf_new_from_inline (-1, fallback, FALSE, NULL);
    G_GNUC_END_IGNORE_DEPRECATIONS
  }
  logo_width = gdk_pixbuf_get_width (simple->logo);
  logo_height = gdk_pixbuf_get_height (simple->logo);

  cursor = gdk_cursor_new_for_display (gdk_window_get_display (root), GDK_WATCH);

  /* create pango layout */
  description = pango_font_description_from_string (font);
  context = gdk_pango_context_get_for_screen (engine->primary_screen);
  pango_context_set_font_description (context, description);
  metrics = pango_context_get_metrics (context, description, NULL);
  text_height = (pango_font_metrics_get_ascent (metrics)
                 + pango_font_metrics_get_descent (metrics)) / PANGO_SCALE
                + 4;

  simple->area.width = logo_width + 2 * BORDER;
  simple->area.height = logo_height + text_height + 3 * BORDER;
  simple->area.x = (geo.width - simple->area.width) / 2;
  simple->area.y = (geo.height - simple->area.height) / 2;

  simple->layout = pango_layout_new (context);
  simple->textbox.x = BORDER;
  simple->textbox.y = simple->area.height - (text_height + BORDER);
  simple->textbox.width = simple->area.width - 2 * BORDER;
  simple->textbox.height = text_height;

  /* create splash window */
  attr.x                  = simple->area.x;
  attr.y                  = simple->area.y;
  attr.event_mask         = GDK_VISIBILITY_NOTIFY_MASK;
  attr.width              = simple->area.width;
  attr.height             = simple->area.height;
  attr.wclass             = GDK_INPUT_OUTPUT;
  attr.window_type        = GDK_WINDOW_TEMP;
  attr.cursor             = cursor;
  attr.override_redirect  = TRUE;

  simple->window = gdk_window_new (root, &attr, GDK_WA_X | GDK_WA_Y
                                  | GDK_WA_NOREDIR | GDK_WA_CURSOR);

  gdk_window_show (simple->window);

  cr = gdk_cairo_create (simple->window);
  gdk_cairo_set_source_rgba (cr, &simple->bgcolor);

  cairo_rectangle (cr, 0, 0, simple->area.width, simple->area.height);
  cairo_fill (cr);

  cairo_move_to (cr, 0, 0);
  gdk_cairo_set_source_pixbuf (cr, simple->logo, 0, 0);
  cairo_paint (cr);

  gdk_window_add_filter (simple->window, simple_filter, simple);
  gdk_window_show (simple->window);

  /* cleanup */
  g_free (font);
  g_free (path);
  pango_font_description_free (description);
  pango_font_metrics_unref (metrics);
  g_object_unref (cursor);
  g_object_unref (context);
  cairo_destroy (cr);
}


static void
simple_next (XfsmSplashEngine *engine, const gchar *text)
{
  Simple *simple = (Simple *) engine->user_data;
  GdkRGBA shcolor;
  gint tw, th, tx, ty;
  cairo_t *cr;

  pango_layout_set_text (simple->layout, text, -1);
  pango_layout_get_pixel_size (simple->layout, &tw, &th);
  tx = simple->textbox.x + (simple->textbox.width - tw) / 2;
  ty = simple->textbox.y + (simple->textbox.height - th) / 2;

  cr = gdk_cairo_create (simple->window);

  /* re-paint the logo */
  gdk_cairo_set_source_pixbuf (cr, simple->logo, 0, 0);
  cairo_paint (cr);

  gdk_cairo_set_source_rgba (cr, &simple->bgcolor);
  cairo_rectangle (cr,
                   simple->textbox.x,
                   simple->textbox.y,
                   simple->textbox.width,
                   simple->textbox.height);
  cairo_fill (cr);

  /* draw shadow */
  shcolor.red = (simple->fgcolor.red + simple->bgcolor.red) / 2;
  shcolor.green = (simple->fgcolor.green + simple->bgcolor.green) / 2;
  shcolor.blue = (simple->fgcolor.blue + simple->bgcolor.blue) / 2;
  shcolor.red = (shcolor.red + shcolor.green + shcolor.blue) / 3;
  shcolor.green = shcolor.red;
  shcolor.blue = shcolor.red;

  gdk_cairo_set_source_rgba (cr, &shcolor);
  cairo_move_to (cr, tx + 2, ty + 2);
  pango_cairo_show_layout (cr, simple->layout);

  gdk_cairo_set_source_rgba (cr, &simple->fgcolor);
  cairo_move_to (cr, tx, ty);
  pango_cairo_show_layout (cr, simple->layout);

  cairo_destroy (cr);
}


static int
simple_run (XfsmSplashEngine *engine,
            GtkWidget        *dialog)
{
  Simple *simple = (Simple *) engine->user_data;
  GtkRequisition requisition;
  int result;
  int x;
  int y;

  simple->dialog_active = TRUE;

  gtk_widget_get_preferred_size (dialog, NULL, &requisition);
  x = simple->area.x + (simple->area.width - requisition.width) / 2;
  y = simple->area.y + (simple->area.height - requisition.height) / 2;
  gtk_window_move (GTK_WINDOW (dialog), x, y);
  result = gtk_dialog_run (GTK_DIALOG (dialog));

  simple->dialog_active = FALSE;

  return result;
}


static void
simple_destroy (XfsmSplashEngine *engine)
{
  Simple *simple = (Simple *) engine->user_data;

  gdk_window_remove_filter (simple->window, simple_filter, simple);
  gdk_window_destroy (simple->window);
  g_object_unref (simple->layout);
  g_object_unref (simple->logo);
  g_free (engine->user_data);
}


G_MODULE_EXPORT void
engine_init (XfsmSplashEngine *engine)
{
  Simple *simple;

  simple = g_new0 (Simple, 1);

  engine->user_data = simple;
  engine->setup = simple_setup;
  engine->next = simple_next;
  engine->run = simple_run;
  engine->destroy = simple_destroy;
}



static void
config_toggled (GtkWidget *button, GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget, gtk_toggle_button_get_active (
                            GTK_TOGGLE_BUTTON (button)));
}


static void
config_configure (XfsmSplashConfig *config,
                  GtkWidget        *parent)
{
  gchar       *font;
  gchar       *path;
  gchar       *path_locale = NULL;
  gchar       *colorstr;
  GtkWidget   *dialog;
  GtkWidget   *frame;
  GtkWidget   *btn_font;
  GtkWidget   *table;
  GtkWidget   *label;
  GtkWidget   *sel_bg;
  GtkWidget   *sel_fg;
  GtkWidget   *checkbox;
  GtkWidget   *vbox;
  GtkWidget   *button;
  GtkFileFilter *filter;
  GdkRGBA      color;
  GtkBox      *dbox;
  gchar       *buffer;
  GtkWidget   *bin;

  dialog = gtk_dialog_new_with_buttons (_("Configure Simple..."),
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("_Close"),
                                        GTK_RESPONSE_CLOSE,
                                        NULL);

  dbox = GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));

  frame = xfce_gtk_frame_box_new (_("Font"), &bin);
  gtk_box_pack_start (dbox, frame, FALSE, FALSE, 6);
  gtk_widget_show (frame);

  font = xfsm_splash_rc_read_entry (config->rc, "Font", DEFAULT_FONT);
  btn_font = gtk_font_button_new_with_font (font);
  g_free (font);
  gtk_container_add (GTK_CONTAINER (bin), btn_font);
  gtk_widget_show (btn_font);

  frame = xfce_gtk_frame_box_new (_("Colors"), &bin);
  gtk_box_pack_start (dbox, frame, FALSE, FALSE, 6);
  gtk_widget_show (frame);

  table = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (bin), table);
  gtk_widget_show (table);

  label = gtk_label_new (_("Background color:"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_widget_show (label);

  colorstr = xfsm_splash_rc_read_entry (config->rc, "BgColor", DEFAULT_BGCOLOR);
  gdk_rgba_parse (&color, colorstr);
  g_free (colorstr);
  sel_bg = gtk_color_button_new_with_rgba (&color);
  gtk_grid_attach (GTK_GRID (table), sel_bg, 1, 0, 1, 1);
  gtk_widget_show (sel_bg);

  label = gtk_label_new (_("Text color:"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
  gtk_widget_show (label);

  colorstr = xfsm_splash_rc_read_entry (config->rc, "FgColor", DEFAULT_FGCOLOR);
  g_debug ("FgColor %s", colorstr);
  gdk_rgba_parse (&color, colorstr);
  g_free (colorstr);
  sel_fg = gtk_color_button_new_with_rgba (&color);
  gtk_grid_attach (GTK_GRID (table), sel_fg, 1, 1, 1, 1);
  gtk_widget_show (sel_fg);

  frame = xfce_gtk_frame_box_new (_("Image"), &bin);
  gtk_box_pack_start (dbox, frame, FALSE, FALSE, 6);
  gtk_widget_show (frame);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (bin), vbox);
  gtk_widget_show (vbox);

  checkbox = gtk_check_button_new_with_label (_("Use custom image"));
  gtk_box_pack_start (GTK_BOX (vbox), checkbox, FALSE, FALSE, 0);
  gtk_widget_show (checkbox);

  button = gtk_file_chooser_button_new (_("Choose image..."),
                                        GTK_FILE_CHOOSER_ACTION_OPEN);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Images"));
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (button), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (button), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (button), filter);

  path = xfsm_splash_rc_read_entry (config->rc, "Image", NULL);
  if (path != NULL)
    path_locale = g_filename_from_utf8 (path, -1, NULL, NULL, NULL);
  g_free (path);
  if (path_locale == NULL || !g_file_test (path_locale, G_FILE_TEST_IS_REGULAR))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), FALSE);
      gtk_widget_set_sensitive (button, FALSE);
    }
  else
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);
      gtk_widget_set_sensitive (button, TRUE);
      gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (button), path_locale);
    }
  g_free (path_locale);
  g_signal_connect (G_OBJECT (checkbox), "toggled",
                    G_CALLBACK (config_toggled), button);

  /* run the dialog */
  gtk_dialog_run (GTK_DIALOG (dialog));

  /* store settings */
  xfsm_splash_rc_write_entry (config->rc, "Font",
                              gtk_font_button_get_font_name (GTK_FONT_BUTTON (btn_font)));

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (sel_bg), &color);
  buffer = gdk_rgba_to_string (&color);
  xfsm_splash_rc_write_entry (config->rc, "BgColor", buffer);
  g_free (buffer);

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (sel_fg), &color);
  buffer = gdk_rgba_to_string (&color);
  xfsm_splash_rc_write_entry (config->rc, "FgColor", buffer);
  g_free (buffer);

  path_locale = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (button));
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox))
      && path_locale != NULL && g_file_test (path_locale, G_FILE_TEST_IS_REGULAR))
    {
      path = g_filename_to_utf8 (path_locale, -1, NULL, NULL, NULL);
      xfsm_splash_rc_write_entry (config->rc, "Image", path);
      g_free (path);
    }
  else
    {
      xfsm_splash_rc_write_entry (config->rc, "Image", "");
    }
  g_free (path_locale);

  gtk_widget_destroy (dialog);
}


static GdkPixbuf*
config_preview (XfsmSplashConfig *config)
{
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* TODO: use GResource or load it as a normal pixbuf */
  return gdk_pixbuf_new_from_inline (-1, preview, FALSE, NULL);
  G_GNUC_END_IGNORE_DEPRECATIONS
}


G_MODULE_EXPORT void
config_init (XfsmSplashConfig *config)
{
  config->name        = g_strdup (_("Simple"));
  config->description = g_strdup (_("Simple Splash Engine"));
  config->version     = g_strdup (VERSION);
  config->author      = g_strdup ("Benedikt Meurer");
  config->homepage    = g_strdup ("http://www.xfce.org/");

  config->configure   = config_configure;
  config->preview     = config_preview;
}


