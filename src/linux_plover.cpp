#include "includes.h"

#include "Renderer.h"
#include "ttfRenderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

// Globals
global_var Renderer renderer{};

// OS-Independent Wrappers
// TODO(oliver): Handle errors instead of just asserting
void readFile(const char *path, u8 **buffer, u64 *bufferSize) {
	FILE *fp = fopen(path, "r");
	if (fp != NULL) {
		if (fseek(fp, 0L, SEEK_END) == 0) {
			*bufferSize = ftell(fp);
			assert(*bufferSize != -1);

			*buffer = new u8[sizeof(u8) * (*bufferSize + 1)];
			assert(fseek(fp, 0L, SEEK_SET) == 0);

		    *bufferSize = fread(*buffer, sizeof(u8), *bufferSize, fp);
			assert(ferror(fp) == 0);

			(*buffer)[(*bufferSize)++] = '\0';
		}
	}
	fclose(fp);
}

// Shared Library Loading

struct linux_GameCode {
	f_gameUpdateAndRender* updateAndRender;
};

internal_func int linux_guarStub(Handles *_h, GameMemory *_gm) {
	printf("Shared lib not loaded\n");
	return 0;
}

internal_func linux_GameCode linux_loadGameCode() {
	linux_GameCode result = {};
	result.updateAndRender = linux_guarStub;
	void *gameCodeSO = dlopen("../libProjectG.so", RTLD_LAZY);
	if (gameCodeSO) {
		result.updateAndRender = (f_gameUpdateAndRender*) dlsym(gameCodeSO, "gameUpdateAndRender");
		if (!result.updateAndRender) result.updateAndRender = linux_guarStub;
	}

	return result;
}

// Handles
void linux_DEBUG_log(const char *str) {
	printf("%s", str);
}

void linux_pushRenderMessage(RenderMessage inMsg) {
	return renderer.msgQueue.push(inMsg);
}

bool linux_hasRenderResult() {
	return renderer.resultQueue.hasMessage();
}

RenderResult linux_popRenderResult() {
	return renderer.resultQueue.pop();
}

internal_func Handles linux_createHandles() {
	Handles handles{};
	handles.DEBUG_log = linux_DEBUG_log;
	handles.pushRenderMessage = linux_pushRenderMessage;
	handles.hasRenderResult = linux_hasRenderResult;
	handles.popRenderResult = linux_popRenderResult;
	return handles;
}

// Memory

internal_func GameMemory linux_createMemory() {
	GameMemory memory{};
	memory.initialized = false;
	memory.persistentStorageSize = Megabytes(64);
	memory.persistentStorage = calloc(1, memory.persistentStorageSize);

	return memory;
}

internal_func void linux_destroyMemory(GameMemory memory) {
	free(memory.persistentStorage);
}

int main() {
	linux_GameCode game = linux_loadGameCode();
	Handles handles = linux_createHandles();
	GameMemory memory = linux_createMemory();

	// Initialize font rendering
	if (!ttfInit()) {
		exit(-1);
	}

	renderer.init();
	while (renderer.render()) {
		game.updateAndRender(&handles, &memory);
		renderer.processMessages();
	}
	renderer.cleanup();

	linux_destroyMemory(memory);
	return 0;
}
