export const id = "EVENT_LOAD_PINBALL_FLIPPERS";
export const name = "Load pinball flippers";
export const groups = ["Pinball"];

const wrap8Bit = (val) => (256 + (val % 256)) % 256;
const decHex = (dec) =>
  `0x${wrap8Bit(dec).toString(16).padStart(2, "0").toUpperCase()}`;
  
const KEY_BITS = {
  left: 0x02,
  right: 0x01,
  up: 0x04,
  down: 0x08,
  a: 0x10,
  b: 0x20,
  select: 0x40,
  start: 0x80,
};

const inputDec = (input) => {
  let output = 0;
  if (Array.isArray(input)) {
    for (let i = 0; i < input.length; i++) {
      output |= KEY_BITS[input[i]];
    }
  } else {
    output = KEY_BITS[input];
  }
  return output;
};

export const autoLabel = (fetchArg) => {
  return `Load pinball flippers`;
};

export const fields = [].concat(
  [
    {
      key: "flipperCount",
      label: "Amount of flippers",
      description: "Amount of flippers",
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
        key: "flipperCount",
        gt: i,
      },
    ],
    fields: [
	  {
		  key: `flipper_idx_${i}`,
          label: `Flipper #${i + 1}`,
		  type: "collapsable",
      },
	  {
		  type: "group",
		  wrapItems: true,
		  conditions: [
			{
				key: `flipper_idx_${i}`,
				ne: true,
			},
		  ],
		  fields: [
			{
				key: `input_${i}`,
				label: "Input",
				description: "Input",
				type: "input",
				defaultValue: [],
			},
			{
				key: `actor_idx_${i}`,
				label: "Actor",
				description: "Actor",
				type: "actor",
				defaultValue: "$self$",
			},
			{
				key: `flipper_start_circle_x_${i}`,
				label: "Start Circle Center X",
				description: "Start Circle Center X",
				type: "number",
				min: -128,
				max: 127,
				defaultValue: 0,
			},
			{
				key: `flipper_start_circle_y_${i}`,
				label: "Start Circle Center Y",
				description: "Start Circle Center Y",
				type: "number",
				min: -128,
				max: 127,
				defaultValue: -8,
			},
			{
				key: `flipper_start_circle_radius_${i}`,
				label: "Start Circle Radius",
				description: "Start Circle Radius",
				type: "number",
				min: 1,
				max: 127,
				defaultValue: 4,
			},
			{
				key: `flipper_top_line_start_x_${i}`,
				label: "Top Line Start X",
				description: "Top Line Start X",
				type: "number",
				min: -128,
				max: 127,
				defaultValue: 0,
			},
			{
				key: `flipper_top_line_start_y_${i}`,
				label: "Top Line Start Y",
				description: "Top Line Start Y",
				type: "number",
				min: -128,
				max: 127,
				defaultValue: -16,
			},
			{
				key: `flipper_top_line_end_x_${i}`,
				label: "Top Line End X",
				description: "Top Line End X",
				type: "number",
				min: -128,
				max: 127,
				defaultValue: 16,
			},
			{
				key: `flipper_top_line_end_y_${i}`,
				label: "Top Line End Y",
				description: "Top Line End Y",
				type: "number",
				min: -128,
				max: 127,
				defaultValue: -8,
			},
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
   
  let flippers = "";
  
  for (let i = 0; i < input.flipperCount; i++){	 
	  flippers += `{
		.actor_idx = ${decHex(getActorIndex(input[`actor_idx_${i}`]))},
        .start_circle = {
            .center_x = ${decHex(input[`flipper_start_circle_x_${i}`])},
            .center_y = ${decHex(input[`flipper_start_circle_y_${i}`])},
			.radius = ${decHex(input[`flipper_start_circle_radius_${i}`])}
        },
        .top_line = {
			.start_x = ${decHex(input[`flipper_top_line_start_x_${i}`])},
			.start_y = ${decHex(input[`flipper_top_line_start_y_${i}`])},
			.end_x = ${decHex(input[`flipper_top_line_end_x_${i}`])},
			.end_y = ${decHex(input[`flipper_top_line_end_y_${i}`])},
		},
		.input = ${decHex(inputDec(input[`input_${i}`]))},
	  },\n`;
  }
  
  const flippers_symbol = _getAvailableSymbol(scene.symbol + "_pinball_flippers");
  
  writeAsset(
      `${flippers_symbol}.c`,
      `#pragma bank 255
	  
	  #include <string.h>
	  #include "data/${flippers_symbol}.h"
	  #include "vm.h"
	  #include "states/pinball.h"
	  
	  BANKREF(${flippers_symbol})
	  
	  const flipper_t ${flippers_symbol}[] = {
	 	 ${flippers}
	  };`
	);
	  
  writeAsset(
	  `${flippers_symbol}.h`,
	  `#ifndef __${flippers_symbol}_INCLUDE__
	  #define __${flippers_symbol}_INCLUDE__
	  
	  #include <gbdk/platform.h>
	  #include "states/pinball.h"
	  	  	  
	  BANKREF_EXTERN(${flippers_symbol})
	  extern const flipper_t ${flippers_symbol}[];
	  
	  #endif
	  `
    );
  
  _addComment("Load pinball flippers");
  
  _stackPushConst(input.flipperCount);
  _stackPushConst(`_${flippers_symbol}`);
  _stackPushConst(`___bank_${flippers_symbol}`);
  		
  _callNative("vm_load_pinball_flippers");
  _stackPop(3);  
  
};
