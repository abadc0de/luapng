#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <png.h>
#include <stdlib.h>

int luapng_load(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TSTRING);
    const char *file_name = lua_tostring(L, 1);
    
    png_byte header[8];

    FILE *fp = fopen(file_name, "rb");
    if (fp == 0)
    {
        return luaL_error(L, "can't open %s", file_name);
    }

    // read the header
    fread(header, 1, 8, fp);

    if (png_sig_cmp(header, 0, 8))
    {
        fclose(fp);
        return luaL_error(L, "%s is not a PNG", file_name);
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        return luaL_error(L, "can't read %s", file_name);
    }

    // create png info struct
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        fclose(fp);
        return luaL_error(L, "can't read info from %s", file_name);
    }

    // create png info struct
    png_infop end_info = png_create_info_struct(png_ptr);
    if (!end_info)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
        fclose(fp);
        return luaL_error(L, "can't read info from %s", file_name);
    }

    // the code in this if statement gets called if libpng encounters an error
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return luaL_error(L, "error from libpng while loading %s", file_name);
    }

    // init png reading
    png_init_io(png_ptr, fp);

    // let libpng know you already read the first 8 bytes
    png_set_sig_bytes(png_ptr, 8);

    // read all the info up to the image data
    png_read_info(png_ptr, info_ptr);

    // variables to pass to get info
    int bit_depth, color_type;
    png_uint_32 temp_width, temp_height;

    // get info about png
    png_get_IHDR(png_ptr, info_ptr, &temp_width, &temp_height, &bit_depth,
        &color_type, NULL, NULL, NULL);

    //printf("%s: %lux%lu %d\n", file_name, temp_width, temp_height, color_type);

    if (bit_depth != 8)
    {
        return luaL_error(L, "%s: unsupported bit depth %d, must be 8",
            file_name, bit_depth);
    }

    int has_alpha = 0;
    switch(color_type)
    {
    case PNG_COLOR_TYPE_RGB:
        has_alpha = 0;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
        has_alpha = 1;
        break;
    default:
        return luaL_error(L, "%s: unknown libpng color type %d",
            file_name, color_type);
    }

    // Update the png info struct.
    png_read_update_info(png_ptr, info_ptr);

    // Row size in bytes.
    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    // glTexImage2d requires rows to be 4-byte aligned
    rowbytes += 3 - ((rowbytes-1) % 4);

    // Allocate the image_data as a big block, to be given to opengl
    size_t image_data_size = rowbytes * temp_height * sizeof(png_byte) + 15;
    png_byte * image_data = (png_byte *)malloc(image_data_size);
    if (image_data == NULL)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return luaL_error(L, "%s: could not allocate memory for image data",
            file_name);
    }

    // row_pointers is for pointing to image_data for reading the png with libpng
    png_byte ** row_pointers = (png_byte **)malloc(temp_height * sizeof(png_byte *));
    if (row_pointers == NULL)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        free(image_data);
        fclose(fp);
        return luaL_error(L, "%s: could not allocate memory for row pointers",
            file_name);
    }

    // set the individual row_pointers to point at the correct offsets of image_data
    for (unsigned int i = 0; i < temp_height; i++)
    {
        row_pointers[i] = image_data + i * rowbytes;
    }

    // read the png into image_data through row_pointers
    png_read_image(png_ptr, row_pointers);

    // Generate the OpenGL texture object
    lua_pushlstring(L, (char *)image_data, image_data_size);
    lua_pushnumber(L, temp_width);
    lua_pushnumber(L, temp_height);
    lua_pushboolean(L, has_alpha);
    
    // clean up
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    free(image_data);
    free(row_pointers);
    fclose(fp);
    
    return 4;
}

int luaopen_luapng(lua_State *L)
{
    const luaL_Reg api[] = {
        {"load", luapng_load},
        {NULL, NULL}
    };

#if LUA_VERSION_NUM == 501
    luaL_register (L, "luapng", api);
#else
    luaL_newlib (L, api);
#endif
    
    return 1;
}

