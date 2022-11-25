import { SchemaTypes } from "../../schema/types";
import { TypeFilter } from "../../filter/type-filter";
import { JSONSchema } from "./types";

export interface JSContext {
  schema: SchemaTypes;
  typeFilter: TypeFilter;
  baseUri: URL;
  jsonSchema: Map<string, JSONSchema>;
  strictIntTypes: boolean;
  strictUnions: boolean;
  getRefUrl: (fqtn: string[]) => string;
}
