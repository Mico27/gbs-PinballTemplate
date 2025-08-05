export const id = "EVENT_LOAD_COLLISION_MASKS";
export const name = "Load collision masks";
export const groups = ["Pinball"];

export const autoLabel = (fetchArg) => {
  return `Load collision masks`;
};

export const fields = [
  {
    key: "sceneId",
    label: "Collision Scene",
    type: "scene",
	width: "100%",
    defaultValue: "LAST_SCENE",
  }
];

const background_cache = {};

export const compile = (input, helpers) => {
  const { options, _callNative, _stackPushConst, _stackPush, _stackPop, _addComment, _declareLocal, variableSetToScriptValue, writeAsset } = helpers;
  
  const { scenes, scene, engineFields } = options;
  const collision_scene = scenes.find((s) => s.id === input.sceneId);
  if (!collision_scene) {
    return;
  }  
  
  if (!background_cache[collision_scene.backgroundId]){
	const oldTilemapData = collision_scene.background.tilemap.data;
	//const oldTilemapAttrData = collision_scene.background.tilemapAttr?.data;
	const tilesetData = collision_scene.background.tileset.data;
	//const cgbTilesetData = collision_scene.background.cgbTileset?.data;
	
	const n_tiles = tilesetData.length;
	const ui_reserved_offset = (n_tiles > 128 && n_tiles < 192)? (192 - n_tiles): 0;
	if (ui_reserved_offset){
		for (let i = 0; i < oldTilemapData.length; i++){
			if (ui_reserved_offset && oldTilemapData[i] >= 128){
				oldTilemapData[i] = oldTilemapData[i] - ui_reserved_offset;
			}
		}
	}
	background_cache[scene.backgroundId] = true;
  }
  
  _addComment("Load collision");
  
  _stackPushConst(`_${collision_scene.symbol}`);
  _stackPushConst(`___bank_${collision_scene.symbol}`);
  		
  _callNative("vm_load_collision_masks");
  _stackPop(2);  
  
};
