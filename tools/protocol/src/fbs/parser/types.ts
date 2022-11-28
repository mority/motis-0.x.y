import { choice, Parser, recursiveParser, str } from "arcsecond";
import {
  BasicType,
  FLOAT_TYPE_PROPERTIES,
  FLOAT_TYPES,
  FloatTypeName,
  INT_TYPE_PROPERTIES,
  INT_TYPES,
  IntTypeName,
} from "../basic_types";
import { AstCustomType, AstFieldType } from "../ast";
import { betweenBrackets } from "./helpers";
import { identWithOptionalNamespace } from "./constants";

export const boolType: Parser<BasicType> = str("bool").map(() => {
  return { sc: "bool", type: "bool" };
});

export const stringType: Parser<BasicType> = str("string").map(() => {
  return { sc: "string", type: "string" };
});

export const intType: Parser<BasicType> = choice(INT_TYPES.map(str)).map(
  (x) => {
    return { sc: "int", type: x, ...INT_TYPE_PROPERTIES[x as IntTypeName] };
  }
);

export const floatType: Parser<BasicType> = choice(FLOAT_TYPES.map(str)).map(
  (x) => {
    return {
      sc: "float",
      type: x,
      ...FLOAT_TYPE_PROPERTIES[x as FloatTypeName],
    };
  }
);

export const basicType: Parser<BasicType> = choice([
  boolType,
  stringType,
  intType,
  floatType,
]);

export const customType: Parser<AstCustomType> = identWithOptionalNamespace.map(
  (x) => {
    return { c: "custom", type: x };
  }
);

export const type: Parser<AstFieldType> = recursiveParser(() =>
  choice([
    basicType.map((x) => {
      return { c: "basic", type: x };
    }),
    betweenBrackets(type).map((x) => {
      return { c: "vector", type: x as AstFieldType };
    }),
    customType,
  ])
);
