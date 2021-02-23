#include "t3f/t3f.h"

int file_size(const char * fn)
{
	ALLEGRO_FS_ENTRY * entry;
	int size = 0;

	entry = al_create_fs_entry(fn);
	if(entry)
	{
		size = al_get_fs_entry_size(entry);
		al_destroy_fs_entry(entry);
	}
	return size;
}

bool embed_file(ALLEGRO_FILE * fp, const char * fn)
{
	ALLEGRO_FILE * efp;
	int i;

	efp = al_fopen(fn, "rb");
	if(efp)
	{
		for(i = 0; i < al_fsize(efp); i++)
		{
			al_fputc(fp, al_fgetc(efp));
		}
		al_fclose(efp);
		return true;
	}
	return false;
}

bool create_windows_icon(ALLEGRO_BITMAP ** bp, ALLEGRO_PATH * path)
{
	ALLEGRO_FILE * fp;
	int i;
	int count = 0;
	int w, h;
	char buf[32];
	int size[256] = {0};
	int current_size = 0;

	for(i = 0; bp[i]; i++)
	{
		count++;
	}
	for(i = 0; i < count; i++)
	{
		sprintf(buf, "%d.png", i);
		al_save_bitmap(buf, bp[i]);
		size[i] = file_size(buf);
	}
	fp = al_fopen(al_path_cstr(path, '/'), "wb");
	if(fp)
	{
		/* icon header */
		al_fwrite16le(fp, 0);
		al_fwrite16le(fp, 1);
		al_fwrite16le(fp, count);

		/* image directory */
		for(i = 0; i < count; i++)
		{
			w = al_get_bitmap_width(bp[i]);
			h = al_get_bitmap_height(bp[i]);
			al_fputc(fp, w < 256 ? w : 0);
			al_fputc(fp, h < 256 ? h : 0);
			al_fputc(fp, 0);
			al_fputc(fp, 0);
			al_fwrite16le(fp, 0);
			al_fwrite16le(fp, 32);
			al_fwrite32le(fp, size[i]);
			al_fwrite32le(fp, 6 + 16 * count + current_size);
			current_size += size[i];
		}
		for(i = 0; i < count; i++)
		{
			sprintf(buf, "%d.png", i);
			embed_file(fp, buf);
			al_remove_filename(buf);
		}
		al_fclose(fp);
		return true;
	}
	return false;
}

static ALLEGRO_PATH * get_output_path(const char * initial)
{
	ALLEGRO_FILECHOOSER * file_chooser;
	ALLEGRO_PATH * path;

	file_chooser = al_create_native_file_dialog(initial, "Enter icon filename", "*.ico", ALLEGRO_FILECHOOSER_SAVE);
	if(!file_chooser)
	{
		goto fail;
	}
	if(!al_show_native_file_dialog(t3f_display, file_chooser))
	{
		goto fail;
	}
	if(al_get_native_file_dialog_count(file_chooser) < 1)
	{
		goto fail;
	}
	path = al_create_path(al_get_native_file_dialog_path(file_chooser, 0));
	al_destroy_native_file_dialog(file_chooser);
	if(path)
	{
		al_set_path_extension(path, ".ico");
	}

	return path;

	fail:
	{
		if(file_chooser)
		{
			al_destroy_native_file_dialog(file_chooser);
		}
		return NULL;
	}
}

static void center_bitmap(ALLEGRO_BITMAP * src, ALLEGRO_BITMAP * dest, float scale)
{
	ALLEGRO_STATE old_state;
	ALLEGRO_TRANSFORM identity;
	float w, h;
	int ox = 0, oy = 0;

	w = (float)al_get_bitmap_width(src) * scale;
	h = (float)al_get_bitmap_height(src) * scale;
	if(al_get_bitmap_width(src) > al_get_bitmap_height(src))
	{
		oy = (w - h) / 2;
	}
	else
	{
		ox = (h - w) / 2;
	}
	al_store_state(&old_state, ALLEGRO_STATE_TRANSFORM | ALLEGRO_STATE_TARGET_BITMAP);
	al_identity_transform(&identity);
	al_set_target_bitmap(dest);
	al_use_transform(&identity);
	al_clear_to_color(al_map_rgba_f(0.0, 0.0, 0.0, 0.0));
	al_draw_scaled_bitmap(src, 0, 0, al_get_bitmap_width(src), al_get_bitmap_height(src), ox, oy, w, h, 0);
	al_restore_state(&old_state);
}

static ALLEGRO_BITMAP * load_image_resize(const char * fn)
{
	ALLEGRO_BITMAP * loaded_bitmap = NULL;
	ALLEGRO_BITMAP * resized_bitmap = NULL;
	float scale = 1.0;
	int size;

	loaded_bitmap = al_load_bitmap(fn);
	if(!loaded_bitmap)
	{
		goto fail;
	}
	size = al_get_bitmap_width(loaded_bitmap);
	if(al_get_bitmap_height(loaded_bitmap) > size)
	{
		size = al_get_bitmap_height(loaded_bitmap);
	}
	if(size > 256)
	{
		scale = 256.0 / (float)size;
		size = 256;
		printf("size: %d scale: %f\n", size, scale);
	}
	resized_bitmap = al_create_bitmap(size, size);
	if(!resized_bitmap)
	{
		goto fail;
	}
	center_bitmap(loaded_bitmap, resized_bitmap, scale);
	al_destroy_bitmap(loaded_bitmap);

	return resized_bitmap;

	fail:
	{
		if(loaded_bitmap)
		{
			al_destroy_bitmap(loaded_bitmap);
		}
		if(resized_bitmap)
		{
			al_destroy_bitmap(resized_bitmap);
		}
	}
	return NULL;
}

bool process_arguments(int argc, char * argv[], char * used)
{
	ALLEGRO_PATH * output_path = NULL;
	ALLEGRO_BITMAP * bitmap[256] = {NULL};
	int pos = 0;
	int i;

	for(i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "-o"))
		{
			if(i < argc - 1)
			{
				output_path = al_create_path(argv[i + 1]);
				if(!output_path)
				{
					printf("Could not allocate memory for output path!\n");
					return false;
				}
				used[i] = 1;
				used[i + 1] = 1;
			}
		}
	}
	for(i = 1; i < argc; i++)
	{
		if(!used[i])
		{
			bitmap[pos] = load_image_resize(argv[i]);
			if(!bitmap[pos])
			{
				goto fail;
			}
			pos++;
		}
	}
	if(pos <= 0)
	{
		printf("Usage: makeicon ... -o <output filename>\n\n");
		goto fail;
	}
	if(!output_path)
	{
		output_path = get_output_path(NULL);
	}
	if(!output_path)
	{
		return false;
	}
	if(!strcmp(al_get_path_extension(output_path), ".ico"))
	{
		if(!create_windows_icon(bitmap, output_path))
		{
			printf("Failed to create %s!\n", al_path_cstr(output_path, '/'));
			goto fail;
		}
	}
	return true;

	fail:
	{
		for(i = 0; i < pos; i++)
		{
			if(bitmap[i])
			{
				al_destroy_bitmap(bitmap[i]);
			}
		}
		return false;
	}
}

bool init(int argc, char * argv[])
{
	if(!t3f_initialize(T3F_APP_TITLE, 640, 480, 60.0, NULL, NULL, T3F_DEFAULT, NULL))
	{
		printf("Error initializing T3F\n");
		return false;
	}
	al_set_new_bitmap_flags(ALLEGRO_MIN_LINEAR | ALLEGRO_MAG_LINEAR | ALLEGRO_NO_PREMULTIPLIED_ALPHA);

	return true;
}

int main(int argc, char * argv[])
{
	char * used;
	int ret = 0;

	if(!init(argc, argv))
	{
		printf("Failed to initialize program!\n");
		return -1;
	}

	used = malloc(argc);
	if(used)
	{
		memset(used, 0, argc);
		if(!process_arguments(argc, argv, used))
		{
			printf("Failed to complete task!\n");
			ret = -1;
		}
		free(used);
	}
	t3f_finish();

	return ret;
}
