/*
 *  resource_manager.cpp - MacOS resource handling for non-Mac platforms
 *
 *  Written in 2000 by Christian Bauer
 */

/*
 *  NOTE:
 *  This module only emulates the "data retrieval" aspect of Mac resources.
 *  It doesn't emulate handles, and the data returned by GetResource() is
 *  a malloc()ed block.
 */

#include <SDL/SDL_endian.h>

#include "cseries.h"
#include "FileHandler.h"
#include "resource_manager.h"

#include <stdio.h>
#include <vector>
#include <list>
#include <map>


// Structure for open resource file
struct res_file_t {
	res_file_t() : f(NULL) {}
	res_file_t(SDL_RWops *file) : f(file) {}
	res_file_t(const res_file_t &other) {f = other.f;}
	~res_file_t() {}

	const res_file_t &operator=(const res_file_t &other)
	{
		if (this != &other)
			f = other.f;
		return *this;
	}

	bool read_map(void);
	int count_resources(uint32 type) const;
	void get_resource_id_list(uint32 type, vector<int> &ids) const;
	void *get_resource(uint32 type, int id, uint32 *size_ret) const;
	void *get_ind_resource(uint32 type, int index, uint32 *size_ret) const;
	bool has_resource(uint32 type, int id) const;

	SDL_RWops *f;		// Opened resource file

	typedef map<int, uint32> id_map_t;			// Maps resource ID to offset to resource data
	typedef map<uint32, id_map_t> type_map_t;	// Maps resource type to ID map

	type_map_t types;	// Map of all resource types found in file
};


// List of open resource files
static list<res_file_t *> res_file_list;
static list<res_file_t *>::iterator cur_res_file_t;


/*
 *  Find file in list of opened files
 */

static list<res_file_t *>::iterator find_res_file_t(SDL_RWops *f)
{
	list<res_file_t *>::iterator i, end = res_file_list.end();
	for (i=res_file_list.begin(); i!=end; i++) {
		res_file_t *r = *i;
		if (r->f == f)
			return i;
	}
	return res_file_list.end();
}


/*
 *  Initialize resource management, open global resource file
 */

void initialize_resources(FileObject &global_resources)
{
	if (OpenResFile(global_resources) == NULL) {
		fprintf(stderr, "Can't open global resource file '%s'\n", global_resources.name.c_str());
		exit(1);
	}
}


/*
 *  Read and parse resource map from file
 */

bool res_file_t::read_map(void)
{
	SDL_RWseek(f, 0, SEEK_END);
	uint32 file_size = SDL_RWtell(f);
	SDL_RWseek(f, 0, SEEK_SET);

	// Read resource header
	uint32 fork_start = 0;
	uint32 data_offset = SDL_ReadBE32(f);
	if (data_offset == 0x00051600) {

		// Looks like an AppleSingle file, look for resource fork entry
		SDL_RWseek(f, 0x18, SEEK_SET);
		int num_entries = SDL_ReadBE16(f);
		bool resource_fork_found = false;
		while (num_entries--) {
			uint32 id = SDL_ReadBE32(f);
			int32 ofs = SDL_ReadBE32(f);
			int32 len = SDL_ReadBE32(f);
#ifdef DEBUG
			printf(" entry id %d, offset %d, length %d\n", id, ofs, len);
#endif
			if (id == 2) {
				resource_fork_found = true;
				fork_start = ofs;
				file_size = ofs + len;
			}
		}
		if (!resource_fork_found) {
			fprintf(stderr, "No resource fork found in AppleSingle file\n");
			return false;
		}
		SDL_RWseek(f, fork_start, SEEK_SET);
		data_offset = fork_start + SDL_ReadBE32(f);
	}
	uint32 map_offset = fork_start + SDL_ReadBE32(f);
	uint32 data_size = SDL_ReadBE32(f);
	uint32 map_size = SDL_ReadBE32(f);
#ifdef DEBUG
	printf(" data_offset %d, map_offset %d, data_size %d, map_size %d\n", data_offset, map_offset, data_size, map_size);
#endif

	// Verify integrity of resource header
	if (data_offset >= file_size || map_offset >= file_size ||
	    data_offset + data_size > file_size || map_offset + map_size > file_size) {
		fprintf(stderr, "Resource header corrupt\n");
		return false;
	}

	// Read map header
	SDL_RWseek(f, map_offset + 24, SEEK_SET);
	uint32 type_list_offset = map_offset + SDL_ReadBE16(f);
	uint32 name_list_offset = map_offset + SDL_ReadBE16(f);
#ifdef DEBUG
	printf(" type_list_offset %d, name_list_offset %d\n", type_list_offset, name_list_offset);
#endif

	// Verify integrity of map header
	if (type_list_offset >= file_size) {
		fprintf(stderr, "Resource map header corrupt\n");
		return false;
	}

	// Read resource type list
	SDL_RWseek(f, type_list_offset, SEEK_SET);
	int num_types = SDL_ReadBE16(f) + 1;
	for (int i=0; i<num_types; i++) {

		// Read type list item
		uint32 type = SDL_ReadBE32(f);
		int num_refs = SDL_ReadBE16(f) + 1;
		uint32 ref_list_offset = type_list_offset + SDL_ReadBE16(f);
#ifdef DEBUG
		printf("  type %c%c%c%c, %d refs\n", type >> 24, type >> 16, type >> 8, type, num_refs);
#endif

		// Verify integrity of item
		if (ref_list_offset >= file_size) {
			fprintf(stderr, "Resource type list corrupt\n");
			return false;
		}

		// Create ID map for this type
		id_map_t &id_map = types[type];

		// Read reference list
		uint32 cur = SDL_RWtell(f);
		SDL_RWseek(f, ref_list_offset, SEEK_SET);
		for (int j=0; j<num_refs; j++) {

			// Read list item
			int id = SDL_ReadBE16(f);
			SDL_RWseek(f, 2, SEEK_CUR);
			uint32 rsrc_data_offset = data_offset + (SDL_ReadBE32(f) & 0x00ffffff);
#ifdef DEBUG
//			printf("   id %d, rsrc_data_offset %d\n", id, rsrc_data_offset);
#endif

			// Verify integrify of item
			if (rsrc_data_offset >= file_size) {
				fprintf(stderr, "Resource reference list corrupt\n");
				return false;
			}

			// Add ID to map
			id_map[id] = rsrc_data_offset;

			SDL_RWseek(f, 4, SEEK_CUR);
		}
		SDL_RWseek(f, cur, SEEK_SET);
	}

	return true;
}


/*
 *  Open resource file, set current file to the newly opened one
 */

SDL_RWops *OpenResFile(FileObject &file)
{
printf("OpenResFile %s\n", file.name.c_str());
	FileObject rsrc_file = file;
	rsrc_file.name += ".resources";

	// Open file, try <name>.resources first, then <name>
	SDL_RWops *f = SDL_RWFromFile(rsrc_file.name.c_str(), "rb");
	if (f == NULL)
		f = SDL_RWFromFile(file.name.c_str(), "rb");
	if (f) {

		// Successful, create res_file_t object and read resource map
		res_file_t *r = new res_file_t(f);
		if (r->read_map()) {

			// Successful, add file to list of open files
			res_file_list.push_back(r);
			cur_res_file_t = --res_file_list.end();

		} else {

			// Error reading resource map
			fprintf(stderr, "Error reading resource map of '%s'\n", file.name.c_str());
			SDL_FreeRW(f);
			return NULL;
		}
	}
	return f;
}


/*
 *  Close resource file
 */

void CloseResFile(SDL_RWops *file)
{
	if (file == NULL)
		return;

	// Find file in list
	list<res_file_t *>::iterator i = find_res_file_t(file);
	assert(i != res_file_list.end());

	// Remove it from the list, close the file and delete the res_file_t
	res_file_list.erase(i);
	res_file_t *r = *i;
	SDL_FreeRW(r->f);
	delete r;

	cur_res_file_t = --res_file_list.end();
}


/*
 *  Return current resource file
 */

SDL_RWops *CurResFile(void)
{
	res_file_t *r = *cur_res_file_t;
	assert(r);
	return r->f;
}


/*
 *  Set current resource file
 */

void UseResFile(SDL_RWops *file)
{
	list<res_file_t *>::iterator i = find_res_file_t(file);
	assert(i != res_file_list.end());
	cur_res_file_t = i;
}


/*
 *  Count number of resources of given type
 */

int res_file_t::count_resources(uint32 type) const
{
	type_map_t::const_iterator i = types.find(type);
	if (i == types.end())
		return 0;
	else
		return i->second.size();
}

int Count1Resources(uint32 type)
{
	return (*cur_res_file_t)->count_resources(type);
}

int CountResources(uint32 type)
{
	int count = 0;
	list<res_file_t *>::const_iterator i = cur_res_file_t, begin = res_file_list.begin();
	while (true) {
		count += (*i)->count_resources(type);
		if (i == begin)
			break;
		i--;
	}
	return count;
}


/*
 *  Get list of id of resources of given type
 */

void res_file_t::get_resource_id_list(uint32 type, vector<int> &ids) const
{
	type_map_t::const_iterator i = types.find(type);
	if (i != types.end()) {
		id_map_t::const_iterator j, end = i->second.end();
		for (j=i->second.begin(); j!=end; j++)
			ids.push_back(j->first);
	}
}

void Get1ResourceIDList(uint32 type, vector<int> &ids)
{
	ids.clear();
	(*cur_res_file_t)->get_resource_id_list(type, ids);
}

void GetResourceIDList(uint32 type, vector<int> &ids)
{
	ids.clear();
	list<res_file_t *>::const_iterator i = cur_res_file_t, begin = res_file_list.begin();
	while (true) {
		(*i)->get_resource_id_list(type, ids);
		if (i == begin)
			break;
		i--;
	}
}


/*
 *  Get resource data (must be freed with free())
 */

void *res_file_t::get_resource(uint32 type, int id, uint32 *size_ret) const
{
	// Find resource in map
	type_map_t::const_iterator i = types.find(type);
	if (i != types.end()) {
		id_map_t::const_iterator j = i->second.find(id);
		if (j != i->second.end()) {

			// Found, read data size
			SDL_RWseek(f, j->second, SEEK_SET);
			uint32 size = SDL_ReadBE32(f);

			// Allocate memory and read data
			void *p = malloc(size);
			if (p == NULL)
				return NULL;
			SDL_RWread(f, p, 1, size);
			if (size_ret)
				*size_ret = size;
#ifdef DEBUG
			printf("get_resource type %c%c%c%c, id %d -> data %p, size %d\n", type >> 24, type >> 16, type >> 8, type, id, p, size);
#endif
			return p;
		}
	}
	return NULL;
}

void *Get1Resource(uint32 type, int id, uint32 *size_ret)
{
	return (*cur_res_file_t)->get_resource(type, id, size_ret);
}

void *GetResource(uint32 type, int id, uint32 *size_ret)
{
	list<res_file_t *>::const_iterator i = cur_res_file_t, begin = res_file_list.begin();
	while (true) {
		void *data = (*i)->get_resource(type, id, size_ret);
		if (data)
			return data;
		if (i == begin)
			break;
		i--;
	}
	return NULL;
}


/*
 *  Get resource data by index (must be freed with free())
 */

void *res_file_t::get_ind_resource(uint32 type, int index, uint32 *size_ret) const
{
	// Find resource in map
	type_map_t::const_iterator i = types.find(type);
	if (i != types.end()) {
		if (index < 1 || index > i->second.size())
			return NULL;
		id_map_t::const_iterator j = i->second.begin();
		for (int k=1; k<index; k++)
			++j;

		// Read data size
		SDL_RWseek(f, j->second, SEEK_SET);
		uint32 size = SDL_ReadBE32(f);

		// Allocate memory and read data
		void *p = malloc(size);
		if (p == NULL)
			return NULL;
		SDL_RWread(f, p, 1, size);
		if (size_ret)
			*size_ret = size;
#ifdef DEBUG
		printf("get_ind_resource type %c%c%c%c, index %d -> data %p, size %d\n", type >> 24, type >> 16, type >> 8, type, index, p, size);
#endif
		return p;
	}
	return NULL;
}

void *Get1IndResource(uint32 type, int index, uint32 *size_ret)
{
	return (*cur_res_file_t)->get_ind_resource(type, index, size_ret);
}

void *GetIndResource(uint32 type, int index, uint32 *size_ret)
{
	list<res_file_t *>::const_iterator i = cur_res_file_t, begin = res_file_list.begin();
	while (true) {
		void *data = (*i)->get_ind_resource(type, index, size_ret);
		if (data)
			return data;
		if (i == begin)
			break;
		i--;
	}
	return NULL;
}


/*
 *  Check if resource is present
 */

bool res_file_t::has_resource(uint32 type, int id) const
{
	type_map_t::const_iterator i = types.find(type);
	if (i != types.end()) {
		id_map_t::const_iterator j = i->second.find(id);
		if (j != i->second.end())
			return true;
	}
	return false;
}

bool Has1Resource(uint32 type, int id)
{
	return (*cur_res_file_t)->has_resource(type, id);
}

bool HasResource(uint32 type, int id)
{
	list<res_file_t *>::const_iterator i = cur_res_file_t, begin = res_file_list.begin();
	while (true) {
		if ((*i)->has_resource(type, id))
			return true;
		if (i == begin)
			break;
		i--;
	}
	return false;
}
