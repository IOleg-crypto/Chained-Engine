using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Text;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Chained.Managed.Generator
{
    // ---------------------------------------------------------------------------
    // Syntax receiver: collects every class/struct declaration that carries at
    // least one [NativeCall] or [NativeProperty] attribute.
    // ---------------------------------------------------------------------------
    internal sealed class NativeCallSyntaxReceiver : ISyntaxReceiver
    {
        public List<TypeDeclarationSyntax> Candidates { get; } = new();

        public void OnVisitSyntaxNode(SyntaxNode node)
        {
            if (node is TypeDeclarationSyntax tds && tds.AttributeLists.Count > 0)
                Candidates.Add(tds);
        }
    }

    // ---------------------------------------------------------------------------
    // Type mapping helper.
    // ---------------------------------------------------------------------------
    internal static class TypeMap
    {
        private static readonly Dictionary<string, string> s_Map = new()
        {
            { "void",    "void"    },
            { "bool",    "byte"    },
            { "byte",    "byte"    },
            { "sbyte",   "sbyte"   },
            { "short",   "short"   },
            { "ushort",  "ushort"  },
            { "int",     "int"     },
            { "uint",    "uint"    },
            { "long",    "long"    },
            { "ulong",   "ulong"   },
            { "float",   "float"   },
            { "double",  "double"  },
            { "char*",   "char*"   },
            { "Vector2", "Chained.Vector2" },
            { "Vector3", "Chained.Vector3" },
            { "Vector4", "Chained.Vector4" },
            { "Vector2*","Chained.Vector2*" },
            { "Vector3*","Chained.Vector3*" },
            { "Vector4*","Chained.Vector4*" },
        };

        public static string Resolve(string raw)
            => s_Map.TryGetValue(raw.Trim(), out var mapped) ? mapped : raw.Trim();
    }

    // ---------------------------------------------------------------------------
    // Describes one [NativeCall] attribute.
    // ---------------------------------------------------------------------------
    internal sealed class NativeCallInfo
    {
        public string ClassName   { get; set; } = "";
        public string MethodName  { get; set; } = "";
        public string ReturnType  { get; set; } = "void";
        public List<string> Params { get; set; } = new();

        public string DelegatePtrType()
        {
            var sb = new StringBuilder("delegate* unmanaged<");
            foreach (var p in Params)
            {
                sb.Append(p);
                sb.Append(", ");
            }
            sb.Append(ReturnType);
            sb.Append('>');
            return sb.ToString();
        }

        public string FieldName => MethodName + "_Ptr";
    }

    // ---------------------------------------------------------------------------
    // Describes one [NativeProperty] attribute.
    // ---------------------------------------------------------------------------
    internal sealed class NativePropertyInfo
    {
        public string PropertyName  { get; set; } = "";
        public string PropertyType  { get; set; } = "float";
        public string? GetterMethod { get; set; }
        public string? SetterMethod { get; set; }

        public bool IsStructType => PropertyType is "Vector2" or "Vector3" or "Vector4" or "Chained.Vector2" or "Chained.Vector3" or "Chained.Vector4";
        public bool IsBoolType => PropertyType is "bool";
        public bool IsStringType => PropertyType is "string";

        public NativeCallInfo? MakeGetterCallInfo(string className)
        {
            if (string.IsNullOrEmpty(GetterMethod)) return null;

            if (IsStructType)
            {
                // Out-pointer pattern: void Get(ulong entityID, Vector3* outVal)
                var structPtr = TypeMap.Resolve(PropertyType + "*");
                return new NativeCallInfo
                {
                    ClassName = className,
                    MethodName = GetterMethod!,
                    ReturnType = "void",
                    Params = new List<string> { "ulong", structPtr }
                };
            }
            else if (IsStringType)
            {
                // String return pattern: char* Get(ulong entityID)
                return new NativeCallInfo
                {
                    ClassName = className,
                    MethodName = GetterMethod!,
                    ReturnType = "char*",
                    Params = new List<string> { "ulong" }
                };
            }
            else
            {
                // Direct return pattern: float/int/byte Get(ulong entityID)
                var returnType = TypeMap.Resolve(PropertyType);
                return new NativeCallInfo
                {
                    ClassName = className,
                    MethodName = GetterMethod!,
                    ReturnType = returnType,
                    Params = new List<string> { "ulong" }
                };
            }
        }

        public NativeCallInfo? MakeSetterCallInfo(string className)
        {
            if (string.IsNullOrEmpty(SetterMethod)) return null;

            if (IsStructType)
            {
                // In-pointer pattern: void Set(ulong entityID, Vector3* val)
                var structPtr = TypeMap.Resolve(PropertyType + "*");
                return new NativeCallInfo
                {
                    ClassName = className,
                    MethodName = SetterMethod!,
                    ReturnType = "void",
                    Params = new List<string> { "ulong", structPtr }
                };
            }
            else if (IsStringType)
            {
                // String set pattern: void Set(ulong entityID, char* value)
                return new NativeCallInfo
                {
                    ClassName = className,
                    MethodName = SetterMethod!,
                    ReturnType = "void",
                    Params = new List<string> { "ulong", "char*" }
                };
            }
            else
            {
                // Value pattern: void Set(ulong entityID, float/byte val)
                var paramType = TypeMap.Resolve(PropertyType);
                return new NativeCallInfo
                {
                    ClassName = className,
                    MethodName = SetterMethod!,
                    ReturnType = "void",
                    Params = new List<string> { "ulong", paramType }
                };
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Main Generator. Handles both [NativeCall] and [NativeProperty].
    // ---------------------------------------------------------------------------
    [Generator]
    public sealed class NativeCallGenerator : ISourceGenerator
    {
        private const string NativeCallFullName     = "Chained.NativeCallAttribute";
        private const string NativePropertyFullName = "Chained.NativePropertyAttribute";

        public void Initialize(GeneratorInitializationContext context)
        {
            context.RegisterForSyntaxNotifications(() => new NativeCallSyntaxReceiver());
        }

        public void Execute(GeneratorExecutionContext context)
        {
            if (context.SyntaxReceiver is not NativeCallSyntaxReceiver receiver)
                return;

            var callAttrSymbol     = context.Compilation.GetTypeByMetadataName(NativeCallFullName);
            var propertyAttrSymbol = context.Compilation.GetTypeByMetadataName(NativePropertyFullName);

            var callsByType      = new Dictionary<INamedTypeSymbol, List<NativeCallInfo>>(SymbolEqualityComparer.Default);
            var propertiesByType = new Dictionary<INamedTypeSymbol, List<NativePropertyInfo>>(SymbolEqualityComparer.Default);

            foreach (var candidate in receiver.Candidates)
            {
                bool hasTargetAttribute = candidate.AttributeLists
                    .SelectMany(al => al.Attributes)
                    .Any(a => ExtractName(a.Name) is "NativeCall" or "NativeCallAttribute" or "NativeProperty" or "NativePropertyAttribute");

                if (!hasTargetAttribute)
                    continue;

                var model = context.Compilation.GetSemanticModel(candidate.SyntaxTree);
                if (model.GetDeclaredSymbol(candidate) is not INamedTypeSymbol typeSymbol)
                    continue;

                bool isPartial = candidate.Modifiers.Any(SyntaxKind.PartialKeyword);
                if (!isPartial)
                    continue;

                if (!callsByType.TryGetValue(typeSymbol, out var callList))
                {
                    callList = new List<NativeCallInfo>();
                    callsByType[typeSymbol] = callList;
                }

                if (!propertiesByType.TryGetValue(typeSymbol, out var propertyList))
                {
                    propertyList = new List<NativePropertyInfo>();
                    propertiesByType[typeSymbol] = propertyList;
                }

                foreach (var attrList in candidate.AttributeLists)
                {
                    foreach (var attrSyntax in attrList.Attributes)
                    {
                        var attrInfo = model.GetSymbolInfo(attrSyntax);
                        IMethodSymbol? ctor = attrInfo.Symbol as IMethodSymbol
                            ?? attrInfo.CandidateSymbols.OfType<IMethodSymbol>().FirstOrDefault();

                        if (ctor == null)
                            continue;

                        bool isNativeCall = callAttrSymbol != null
                            ? SymbolEqualityComparer.Default.Equals(ctor.ContainingType, callAttrSymbol)
                            : ctor.ContainingType.ToDisplayString() == NativeCallFullName;

                        bool isNativeProperty = propertyAttrSymbol != null
                            ? SymbolEqualityComparer.Default.Equals(ctor.ContainingType, propertyAttrSymbol)
                            : ctor.ContainingType.ToDisplayString() == NativePropertyFullName;

                        if (isNativeCall)
                        {
                            var info = ParseNativeCall(attrSyntax, model);
                            if (info != null)
                                callList.Add(info);
                        }
                        else if (isNativeProperty)
                        {
                            var info = ParseNativeProperty(attrSyntax, model);
                            if (info != null)
                            {
                                propertyList.Add(info);

                                // Synthesize the NativeCallInfos for getter & setter
                                string fullClassName = typeSymbol.ToDisplayString();
                                var getterCall = info.MakeGetterCallInfo(fullClassName);
                                if (getterCall != null) callList.Add(getterCall);

                                var setterCall = info.MakeSetterCallInfo(fullClassName);
                                if (setterCall != null) callList.Add(setterCall);
                            }
                        }
                    }
                }
            }

            // Emit for each type that has either calls or properties
            var allTypes = callsByType.Keys.Union(propertiesByType.Keys, SymbolEqualityComparer.Default).Cast<INamedTypeSymbol>();

            foreach (var typeSymbol in allTypes)
            {
                var calls = callsByType.TryGetValue(typeSymbol, out var cList) ? cList : new List<NativeCallInfo>();
                var props = propertiesByType.TryGetValue(typeSymbol, out var pList) ? pList : new List<NativePropertyInfo>();

                if (calls.Count == 0 && props.Count == 0)
                    continue;

                // Deduplicate calls by FieldName
                var seenCalls = new HashSet<string>();
                var uniqueCalls = new List<NativeCallInfo>();
                foreach (var c in calls)
                {
                    if (seenCalls.Add(c.FieldName))
                        uniqueCalls.Add(c);
                }

                // Deduplicate properties by PropertyName
                var seenProps = new HashSet<string>();
                var uniqueProps = new List<NativePropertyInfo>();
                foreach (var p in props)
                {
                    if (seenProps.Add(p.PropertyName))
                        uniqueProps.Add(p);
                }

                var source   = BuildSource(typeSymbol, uniqueCalls, uniqueProps);
                var hintName = $"{typeSymbol.Name}.NativeCalls.g.cs";
                context.AddSource(hintName, SourceText.From(source, Encoding.UTF8));
            }
        }

        private static NativeCallInfo? ParseNativeCall(AttributeSyntax attrSyntax, SemanticModel model)
        {
            var args = attrSyntax.ArgumentList?.Arguments;
            if (args == null || args.Value.Count < 2)
                return null;

            string? className  = EvalString(args.Value[0].Expression, model);
            string? methodName = EvalString(args.Value[1].Expression, model);
            if (className == null || methodName == null)
                return null;

            var sigArgs = args.Value.Skip(2).ToList();
            string returnType = sigArgs.Count > 0
                ? TypeMap.Resolve(EvalString(sigArgs[0].Expression, model) ?? "void")
                : "void";

            var paramTypes = sigArgs
                .Skip(1)
                .Select(a => TypeMap.Resolve(EvalString(a.Expression, model) ?? "void"))
                .ToList();

            return new NativeCallInfo
            {
                ClassName  = className,
                MethodName = methodName,
                ReturnType = returnType,
                Params     = paramTypes,
            };
        }

        private static NativePropertyInfo? ParseNativeProperty(AttributeSyntax attrSyntax, SemanticModel model)
        {
            var args = attrSyntax.ArgumentList?.Arguments;
            if (args == null || args.Value.Count < 2)
                return null;

            string? propName = EvalString(args.Value[0].Expression, model);
            string? propType = EvalString(args.Value[1].Expression, model);
            if (propName == null || propType == null)
                return null;

            string? getter = args.Value.Count > 2 ? EvalString(args.Value[2].Expression, model) : null;
            string? setter = args.Value.Count > 3 ? EvalString(args.Value[3].Expression, model) : null;

            return new NativePropertyInfo
            {
                PropertyName = propName,
                PropertyType = propType,
                GetterMethod = getter,
                SetterMethod = setter,
            };
        }

        private static string? EvalString(ExpressionSyntax expr, SemanticModel model)
        {
            var constant = model.GetConstantValue(expr);
            if (constant.HasValue && constant.Value is string s)
                return s;

            if (expr is LiteralExpressionSyntax lit && lit.IsKind(SyntaxKind.StringLiteralExpression))
                return lit.Token.ValueText;

            return null;
        }

        private static string ExtractName(NameSyntax name) => name switch
        {
            IdentifierNameSyntax id    => id.Identifier.Text,
            QualifiedNameSyntax  q     => q.Right.Identifier.Text,
            AliasQualifiedNameSyntax a => a.Name.Identifier.Text,
            _                          => name.ToString(),
        };

        private static string BuildSource(INamedTypeSymbol type, List<NativeCallInfo> calls, List<NativePropertyInfo> properties)
        {
            string ns        = type.ContainingNamespace.IsGlobalNamespace ? "" : type.ContainingNamespace.ToDisplayString();
            string className = type.Name;
            string keyword   = type.TypeKind == TypeKind.Struct ? "struct" : "class";

            var sb = new StringBuilder();
            sb.AppendLine("// <auto-generated/>");
            sb.AppendLine("// Generated by Chained.Managed.Generator — do not edit by hand.");
            sb.AppendLine("// Source: NativeCallGenerator.cs");
            sb.AppendLine("#nullable enable");
            sb.AppendLine("#pragma warning disable 0649, 0169  // field never assigned / never used");
            sb.AppendLine("using System;");
            sb.AppendLine("using System.Runtime.InteropServices;");
            sb.AppendLine();

            bool hasNs = !string.IsNullOrEmpty(ns);
            if (hasNs)
            {
                sb.AppendLine($"namespace {ns}");
                sb.AppendLine("{");
            }

            string indent = hasNs ? "    " : "";

            sb.AppendLine($"{indent}public partial {keyword} {className}");
            sb.AppendLine($"{indent}{{");

            // 1. Emit _Ptr fields
            foreach (var call in calls)
            {
                sb.AppendLine($"{indent}    internal static unsafe {call.DelegatePtrType()} {call.FieldName};");
            }

            if (calls.Count > 0 && properties.Count > 0)
                sb.AppendLine();

            // 2. Emit C# properties (getters/setters)
            foreach (var prop in properties)
            {
                string csharpPropType = prop.PropertyType;
                string resolvedType   = TypeMap.Resolve(prop.PropertyType);

                sb.AppendLine($"{indent}    public {csharpPropType} {prop.PropertyName}");
                sb.AppendLine($"{indent}    {{");

                // Getter
                if (!string.IsNullOrEmpty(prop.GetterMethod))
                {
                    string getterPtr = prop.GetterMethod + "_Ptr";
                    if (prop.IsStructType)
                    {
                        sb.AppendLine($"{indent}        get {{ unsafe {{ {resolvedType} val = default; if ({getterPtr} != null) {getterPtr}(Entity.ID, &val); return val; }} }}");
                    }
                    else if (prop.IsBoolType)
                    {
                        sb.AppendLine($"{indent}        get {{ unsafe {{ return {getterPtr} != null && {getterPtr}(Entity.ID) != 0; }} }}");
                    }
                    else if (prop.IsStringType)
                    {
                        sb.AppendLine($"{indent}        get {{ unsafe {{ return {getterPtr} != null ? Marshal.PtrToStringUni(new IntPtr({getterPtr}(Entity.ID))) ?? string.Empty : string.Empty; }} }}");
                    }
                    else
                    {
                        sb.AppendLine($"{indent}        get {{ unsafe {{ return {getterPtr} != null ? {getterPtr}(Entity.ID) : default; }} }}");
                    }
                }

                // Setter
                if (!string.IsNullOrEmpty(prop.SetterMethod))
                {
                    string setterPtr = prop.SetterMethod + "_Ptr";
                    if (prop.IsStructType)
                    {
                        sb.AppendLine($"{indent}        set {{ unsafe {{ if ({setterPtr} != null) {setterPtr}(Entity.ID, &value); }} }}");
                    }
                    else if (prop.IsBoolType)
                    {
                        sb.AppendLine($"{indent}        set {{ unsafe {{ if ({setterPtr} != null) {setterPtr}(Entity.ID, (byte)(value ? 1 : 0)); }} }}");
                    }
                    else if (prop.IsStringType)
                    {
                        sb.AppendLine($"{indent}        set {{ unsafe {{ if ({setterPtr} != null) fixed (char* ptr = value) {setterPtr}(Entity.ID, ptr); }} }}");
                    }
                    else
                    {
                        sb.AppendLine($"{indent}        set {{ unsafe {{ if ({setterPtr} != null) {setterPtr}(Entity.ID, value); }} }}");
                    }
                }

                sb.AppendLine($"{indent}    }}");
            }

            sb.AppendLine($"{indent}}}");

            if (hasNs)
                sb.AppendLine("}");

            return sb.ToString();
        }
    }
}