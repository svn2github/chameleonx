/*
 * Copyright (c) 2005 Apple Computer, Inc.  All Rights Reserved.
 */

#include "libsaio.h"
#include "device_tree.h"

//#if 1
/*
 
 Structures for a Flattened Device Tree 
 */

#define kPropNameLength 32

typedef struct DeviceTreeNodeProperty {
    char                name[kPropNameLength];  // NUL terminated property name
    unsigned long       length;         // Length (bytes) of folloing prop value
    //  unsigned long       value[1];       // Variable length value of property
    // Padded to a multiple of a longword?
} DeviceTreeNodeProperty;

typedef struct OpaqueDTEntry {
    unsigned long       nProperties;    // Number of props[] elements (0 => end)
    unsigned long       nChildren;      // Number of children[] elements
    //  DeviceTreeNodeProperty      props[];// array size == nProperties
    //  DeviceTreeNode      children[];     // array size == nChildren
} DeviceTreeNode;

typedef char DTPropertyNameBuf[32];
/* Entry Name Definitions (Entry Names are C-Strings)*/
enum {
    kDTMaxEntryNameLength           = 31    /* Max length of a C-String Entry Name (terminator not included) */
};

/* length of DTEntryNameBuf = kDTMaxEntryNameLength +1*/
typedef char DTEntryNameBuf[32];
//#endif

#if DEBUG
#define DPRINTF(args...) printf(args)
void
DT__PrintTree(Node *node);
#else
#define DPRINTF(args...)
#endif


#define RoundToLong(x)	(((x) + 3) & ~3)

static struct _DTSizeInfo {
    uint32_t numNodes;
    uint32_t numProperties;
    uint32_t totalPropertySize;
} DTInfo;

#define kAllocSize 4096

static Node *rootNode;

static Node *freeNodes, *allocedNodes;
static Property *freeProperties, *allocedProperties;

static void *
FlattenNodes(Node *node, void *buffer);
#if DEBUG
static void
_PrintTree(Node *node, int level);

#endif

Property *
DT__AddProperty(Node *node, const char *name, uint32_t length, void *value)
{
    Property *prop;
    
    DPRINTF("DT__AddProperty([Node '%s'], '%s', %d, %p)\n", DT__GetName(node), name, length, value);
    if (freeProperties == NULL) {
        void *buf = malloc(kAllocSize);
        if (buf == 0) return 0;
        int i;   
        DPRINTF("Allocating more free properties\n");
        bzero(buf, kAllocSize);
        // Use the first property to record the allocated buffer
        // for later freeing.
        prop = (Property *)buf;
        prop->next = allocedProperties;
        allocedProperties = prop;
        prop->value = buf;
        prop++;
        for (i=1; (unsigned)i<(kAllocSize / sizeof(Property)); i++) {
            prop->next = freeProperties;
            freeProperties = prop;
            prop++;
        }
    }
    prop = freeProperties;
    freeProperties = prop->next;
    
    prop->name = newString(name);
    prop->length = length;
    prop->value = value;
    
    // Always add to end of list
    if (node->properties == 0) {
        node->properties = prop;
    } else {
        node->last_prop->next = prop;
    }
    node->last_prop = prop;
    prop->next = 0;
    
    DPRINTF("Done [%p]\n", prop);
    
    DTInfo.numProperties++;
    DTInfo.totalPropertySize += RoundToLong(length);
    
    return prop;
}

Node *
DT__AddChild(Node *parent, const char *name)
{
    Node *node;
    
    if (freeNodes == NULL) {
        void *buf = malloc(kAllocSize);
        if (buf == 0) return 0;
        int i;        
        DPRINTF("Allocating more free nodes\n");
        bzero(buf, kAllocSize);
        node = (Node *)buf;
        // Use the first node to record the allocated buffer
        // for later freeing.
        node->next = allocedNodes;
        allocedNodes = node;
        node->children = (Node *)buf;
        node++;
        for (i=1; (unsigned)i<(kAllocSize / sizeof(Node)); i++) {
            node->next = freeNodes;
            freeNodes = node;
            node++;
        }
    }
    DPRINTF("DT__AddChild(%p, '%s')\n", parent, name);
    node = freeNodes;
    freeNodes = node->next;
    DPRINTF("Got free node %p\n", node);
    DPRINTF("prop = %p, children = %p, next = %p\n", node->properties, node->children, node->next);
    
    if (parent == NULL) {
        rootNode = node;
        node->next = 0;
    } else {
        node->next = parent->children;
        parent->children = node;
    }
    DTInfo.numNodes++;
    DT__AddProperty(node, "name", strlen(name) + 1, (void *) name);
    return node;
}

void
DT__FreeProperty(Property *prop)
{
    prop->next = freeProperties;
    freeProperties = prop;
}
void
DT__FreeNode(Node *node)
{
    node->next = freeNodes;
    freeNodes = node;
}

void
DT__Initialize(void)
{
    DPRINTF("DT__Initialize\n");
    
    freeNodes = 0;
    allocedNodes = 0;
    freeProperties = 0;
    allocedProperties = 0;
    
    DTInfo.numNodes = 0;
    DTInfo.numProperties = 0;
    DTInfo.totalPropertySize = 0;
    
    rootNode = DT__AddChild(NULL, "/");
    DPRINTF("DT__Initialize done\n");
}

/*
 * Free up memory used by in-memory representation
 * of device tree.
 */
void
DT__Finalize(void)
{
    Node *node;
    Property *prop;
    
    DPRINTF("DT__Finalize\n");
    for (prop = allocedProperties; prop != NULL; prop = prop->next) {
        if (prop->value) free(prop->value);
        if (prop->name) free(prop->name);
		
    }
    allocedProperties = NULL;
    freeProperties = NULL;
    
    for (node = allocedNodes; node != NULL; node = node->next) {
        if (node->children) free((void *)node->children);
    }
    allocedNodes = NULL;
    freeNodes = NULL;
    rootNode = NULL;
    
    // XXX leaks any created strings
    
    DTInfo.numNodes = 0;
    DTInfo.numProperties = 0;
    DTInfo.totalPropertySize = 0;
}

static void *
FlattenNodes(Node *node, void *buffer)
{
    Property *prop;
    DeviceTreeNode *flatNode;
    DeviceTreeNodeProperty *flatProp;
    int count;
    
    if (node == 0) return buffer;
    
    flatNode = (DeviceTreeNode *)buffer;
    buffer += sizeof(DeviceTreeNode);
    
    for (count = 0, prop = node->properties; prop != 0; count++, prop = prop->next) {
        flatProp = (DeviceTreeNodeProperty *)buffer;
        strlcpy(flatProp->name, prop->name, kPropNameLength);
        flatProp->length = prop->length;
        buffer += sizeof(DeviceTreeNodeProperty);
        bcopy(prop->value, buffer, prop->length);
        buffer += RoundToLong(prop->length);
    }
    flatNode->nProperties = count;
    
    for (count = 0, node = node->children; node != 0; count++, node = node->next) {
        buffer = FlattenNodes(node, buffer);
    }
    flatNode->nChildren = count;
    
    return buffer;
}

/*
 * Flatten the in-memory representation of the device tree
 * into a binary DT block.
 * To get the buffer size needed, call with result = 0.
 * To have a buffer allocated for you, call with *result = 0.
 * To use your own buffer, call with *result = &buffer.
 */

void
DT__FlattenDeviceTree(void **buffer_p, uint32_t *length)
{
    uint32_t totalSize;
    void *buf;
    
    DPRINTF("DT__FlattenDeviceTree(%p, %u)\n", buffer_p, *length);
#if DEBUG
    if (buffer_p) DT__PrintTree(rootNode);
#endif
    
    totalSize = DTInfo.numNodes * sizeof(DeviceTreeNode) + 
    DTInfo.numProperties * sizeof(DeviceTreeNodeProperty) +
    DTInfo.totalPropertySize;
    
    DPRINTF("Total size 0x%x\n", totalSize);
    if (buffer_p != 0) {
        if (totalSize == 0) {
            buf = 0;
        } else {
            if (*buffer_p == 0) {
                buf = malloc(totalSize);
            } else {
                buf = *buffer_p;
            }
            if (!buf) {
                *length = 0;
                return;
            }
            bzero(buf, totalSize);
            
            FlattenNodes(rootNode, buf);
        }
        *buffer_p = buf;
    }
    if (length)
        *length = totalSize;
}

char *
DT__GetName(Node *node)
{
    Property *prop;
    
    //DPRINTF("DT__GetName(0x%x)\n", node);
    //DPRINTF("Node properties = 0x%x\n", node->properties);
    for (prop = node->properties; prop; prop = prop->next) {
        //DPRINTF("Prop '%s'\n", prop->name);
        if (strncmp(prop->name, "name",sizeof("name")) == 0) {
            return prop->value;
        }
    }
    //DPRINTF("DT__GetName returns 0\n");
    return "(null)";
}

Node *
DT__FindNode(const char *path, bool createIfMissing)
{
    Node *node, *child = 0;
    DTPropertyNameBuf nameBuf;
    char *bp;
    int i;
    
    DPRINTF("DT__FindNode('%s', %d)\n", path, createIfMissing);
    
    // Start at root
    node = rootNode;
    DPRINTF("root = %p\n", rootNode);
    
    while (node) {
        // Skip leading slash
        while (*path == '/') path++;
        
        for (i=0, bp = nameBuf; ++i < kDTMaxEntryNameLength && *path && *path != '/'; bp++, path++) *bp = *path;
        *bp = '\0';
        
        if (nameBuf[0] == '\0') {
            // last path entry
            break;
        }
        DPRINTF("Node '%s'\n", nameBuf);
        
        for (child = node->children; child != 0; child = child->next) {
            DPRINTF("Child %p\n", child);
            if (strcmp(DT__GetName(child), nameBuf) == 0) {
                break;
            }
        }
        if (child == 0 && createIfMissing) {
            DPRINTF("Creating node\n");     
            
            const char *str = newString(nameBuf);
            if (str) {
                child = DT__AddChild(node, str);
            }
        }
        node = child;
    }
    return node;
}

#if DEBUG

static void
DT__PrintNode(Node *node, int level)
{
    char spaces[10], *cp = spaces;
    Property *prop;
    
    if (level > 9) level = 9;
    while (level--) *cp++ = ' ';
    *cp = '\0';
    
    printf("%s===Node===\n", spaces);
    for (prop = node->properties; prop; prop = prop->next) {
        char c = *((char *)prop->value);
        if (prop->length < 64 && (
                                  strncmp(prop->name, "name",sizeof("name") ) == 0 || 
                                  (c >= '0' && c <= '9') ||
                                  (c >= 'a' && c <= 'z') ||
                                  (c >= 'A' && c <= 'Z') || c == '_')) {
            printf("%s Property '%s' [%d] = '%s'\n", spaces, prop->name, prop->length, prop->value);
        } else {
            printf("%s Property '%s' [%d] = (data)\n", spaces, prop->name, prop->length);
        }
    }
    printf("%s==========\n", spaces);
}

static void
_PrintTree(Node *node, int level)
{
    DT__PrintNode(node, level);
    level++;
    for (node = node->children; node; node = node->next)
        _PrintTree(node, level);
}

void
DT__PrintTree(Node *node)
{
    if (node == 0) node = rootNode;
    _PrintTree(node, 0);
}

#endif

