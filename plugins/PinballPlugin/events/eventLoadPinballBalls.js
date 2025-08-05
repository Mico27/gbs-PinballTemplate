export const id = "EVENT_LOAD_PINBALL_BALLS";
export const name = "Load pinball balls";
export const groups = ["Pinball"];

const wrap8Bit = (val) => (256 + (val % 256)) % 256;
const decHex = (dec) =>
  `0x${wrap8Bit(dec).toString(16).padStart(2, "0").toUpperCase()}`;

export const autoLabel = (fetchArg) => {
  return `Load pinball balls`;
};

export const fields = [].concat(
  [
    {
      key: "ballCount",
      label: "Amount of balls",
      description: "Amount of balls",
      type: "number",
      min: 1,
      max: 4,
      defaultValue: 1,
    }
  ],
  Array(4)
    .fill()
    .reduce((arr, _, i) => {      
      arr.push({
    type: "group",
	wrapItems: true,
    conditions: [
      {
        key: "ballCount",
        gt: i,
      },
    ],
    fields: [
	  {
		  key: `ball_idx_${i}`,
          label: `Ball #${i + 1}`,
		  type: "collapsable",
      },
	  {
		  type: "group",
		  wrapItems: true,
		  conditions: [
			{
				key: `ball_idx_${i}`,
				ne: true,
			},
		  ],
		  fields: [
			{
				key: `actor_idx_${i}`,
				label: "Actor",
				description: "Actor",
				type: "actor",
				defaultValue: "$self$",
			},
			{
				key: `ball_circle_x_${i}`,
				label: "Circle Center X",
				description: "Circle Center X",
				type: "number",
				min: -128,
				max: 127,
				defaultValue: 8,
			},
			{
				key: `ball_circle_y_${i}`,
				label: "Circle Center Y",
				description: "Circle Center Y",
				type: "number",
				min: -128,
				max: 127,
				defaultValue: -8,
			},
			{
				key: `ball_circle_radius_${i}`,
				label: "Circle Radius",
				description: "Circle Radius",
				type: "number",
				min: 1,
				max: 127,
				defaultValue: 4,
			},
			{
				key: `ball_mass_${i}`,
				label: "Mass",
				description: "Mass",
				type: "number",
				min: 0,
				max: 255,
				defaultValue: 4,
			},
			{
				key: `ball_col_layer_${i}`,
				label: "Collision Layer",
				description: "Collision Layer",
				type: "number",
				min: 0,
				max: 255,
				defaultValue: 0,
			}
		],
	  },
    ],
  });
      return arr;
    }, []),  
);

export const compile = (input, helpers) => {
	
  const { options, _callNative, _stackPushConst, _stackPush, _stackPop, _addComment, _declareLocal, variableSetToScriptValue, writeAsset, _getAvailableSymbol, getActorIndex } = helpers;
  
  const { scene, engineFields } = options;
   
  let balls = "";
  
  for (let i = 0; i < input.ballCount; i++){	  
	  balls += `{
		.actor_idx = ${decHex(getActorIndex(input[`actor_idx_${i}`]))},
        .circle = {
            .center_x = ${decHex(input[`ball_circle_x_${i}`])},
            .center_y = ${decHex(input[`ball_circle_y_${i}`])},
			.radius = ${decHex(input[`ball_circle_radius_${i}`])}
        },
        .mass = ${decHex(input[`ball_mass_${i}`])},
        .col_layer = ${decHex(input[`ball_col_layer_${i}`])}
	  },\n`;
  }
  
  const balls_symbol = _getAvailableSymbol(scene.symbol + "_pinball_balls");
  
  writeAsset(
      `${balls_symbol}.c`,
      `#pragma bank 255
	  
	  #include <string.h>
	  #include "data/${balls_symbol}.h"
	  #include "vm.h"
	  #include "states/pinball.h"
	  
	  BANKREF(${balls_symbol})
	  
	  const ball_t ${balls_symbol}[] = {
	 	 ${balls}
	  };`
	);
	  
  writeAsset(
	  `${balls_symbol}.h`,
	  `#ifndef __${balls_symbol}_INCLUDE__
	  #define __${balls_symbol}_INCLUDE__
	  
	  #include <gbdk/platform.h>
	  #include "states/pinball.h"
	  	  	  
	  BANKREF_EXTERN(${balls_symbol})
	  extern const ball_t ${balls_symbol}[];
	  
	  #endif
	  `
    );
  
  _addComment("Load pinball balls");
  
  _stackPushConst(input.ballCount);
  _stackPushConst(`_${balls_symbol}`);
  _stackPushConst(`___bank_${balls_symbol}`);
  		
  _callNative("vm_load_pinball_balls");
  _stackPop(3);  
  
};
