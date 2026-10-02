-module(client).
-export([plain0/2,typed0/2,plain1/2,typed1/2,plain2/2,typed2/2,plain3/2,typed3/2,plain4/2,typed4/2,plain5/2,typed5/2,plain6/2,typed6/2,plain7/2,typed7/2,plain8/2,typed8/2,plain9/2,typed9/2,plain10/2,typed10/2,plain11/2,typed11/2,plain12/2,typed12/2,plain13/2,typed13/2,plain14/2,typed14/2,plain15/2,typed15/2,plain16/2,typed16/2,plain17/2,typed17/2,plain18/2,typed18/2,plain19/2,typed19/2,plain20/2,typed20/2,plain21/2,typed21/2,plain22/2,typed22/2,plain23/2,typed23/2]).
-export([value/0, nested/2]).
value() -> answer:identity(answer:value()).
nested(X, Y) -> answer:first(answer:second(Y, X), answer:identity(Y)).

plain0(X, Y) -> answer:second(answer:identity(X), X).
-spec typed0(integer(), integer()) -> integer().
typed0(X, Y) -> answer:second(answer:identity(X), X).
plain1(X, Y) -> answer:second(answer:identity(answer:first(answer:identity(-47168), X)), answer:first(answer:identity(Y), Y)).
-spec typed1(integer(), integer()) -> integer().
typed1(X, Y) -> answer:second(answer:identity(answer:first(answer:identity(-47168), X)), answer:first(answer:identity(Y), Y)).
plain2(X, Y) -> answer:second(answer:identity(answer:first(answer:identity(answer:second(answer:identity(Y), Y)), answer:first(answer:identity(Y), -47746))), answer:first(answer:identity(answer:second(answer:identity(X), Y)), answer:second(answer:identity(Y), X))).
-spec typed2(integer(), integer()) -> integer().
typed2(X, Y) -> answer:second(answer:identity(answer:first(answer:identity(answer:second(answer:identity(Y), Y)), answer:first(answer:identity(Y), -47746))), answer:first(answer:identity(answer:second(answer:identity(X), Y)), answer:second(answer:identity(Y), X))).
plain3(X, Y) -> answer:first(answer:identity(answer:first(answer:identity(answer:first(answer:identity(answer:first(answer:identity(Y), Y)), answer:first(answer:identity(57754), X))), answer:first(answer:identity(answer:second(answer:identity(60272), -16617)), answer:second(answer:identity(X), X)))), answer:first(answer:identity(answer:first(answer:identity(answer:second(answer:identity(Y), Y)), answer:second(answer:identity(Y), 95036))), answer:first(answer:identity(answer:first(answer:identity(X), X)), answer:second(answer:identity(-49199), Y)))).
-spec typed3(integer(), integer()) -> integer().
typed3(X, Y) -> answer:first(answer:identity(answer:first(answer:identity(answer:first(answer:identity(answer:first(answer:identity(Y), Y)), answer:first(answer:identity(57754), X))), answer:first(answer:identity(answer:second(answer:identity(60272), -16617)), answer:second(answer:identity(X), X)))), answer:first(answer:identity(answer:first(answer:identity(answer:second(answer:identity(Y), Y)), answer:second(answer:identity(Y), 95036))), answer:first(answer:identity(answer:first(answer:identity(X), X)), answer:second(answer:identity(-49199), Y)))).
plain4(X, Y) -> answer:second(answer:identity(X), Y).
-spec typed4(integer(), integer()) -> integer().
typed4(X, Y) -> answer:second(answer:identity(X), Y).
plain5(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(13502), X)), answer:second(answer:identity(X), Y)).
-spec typed5(integer(), integer()) -> integer().
typed5(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(13502), X)), answer:second(answer:identity(X), Y)).
plain6(X, Y) -> answer:second(answer:identity(answer:first(answer:identity(answer:first(answer:identity(X), Y)), answer:first(answer:identity(19439), Y))), answer:first(answer:identity(answer:second(answer:identity(-47418), -38775)), answer:first(answer:identity(Y), X))).
-spec typed6(integer(), integer()) -> integer().
typed6(X, Y) -> answer:second(answer:identity(answer:first(answer:identity(answer:first(answer:identity(X), Y)), answer:first(answer:identity(19439), Y))), answer:first(answer:identity(answer:second(answer:identity(-47418), -38775)), answer:first(answer:identity(Y), X))).
plain7(X, Y) -> answer:second(answer:identity(answer:first(answer:identity(answer:second(answer:identity(answer:second(answer:identity(-74768), Y)), answer:first(answer:identity(37549), 18476))), answer:first(answer:identity(answer:first(answer:identity(Y), Y)), answer:first(answer:identity(X), 43594)))), answer:second(answer:identity(answer:second(answer:identity(answer:first(answer:identity(X), X)), answer:second(answer:identity(Y), Y))), answer:second(answer:identity(answer:second(answer:identity(X), X)), answer:second(answer:identity(12646), Y)))).
-spec typed7(integer(), integer()) -> integer().
typed7(X, Y) -> answer:second(answer:identity(answer:first(answer:identity(answer:second(answer:identity(answer:second(answer:identity(-74768), Y)), answer:first(answer:identity(37549), 18476))), answer:first(answer:identity(answer:first(answer:identity(Y), Y)), answer:first(answer:identity(X), 43594)))), answer:second(answer:identity(answer:second(answer:identity(answer:first(answer:identity(X), X)), answer:second(answer:identity(Y), Y))), answer:second(answer:identity(answer:second(answer:identity(X), X)), answer:second(answer:identity(12646), Y)))).
plain8(X, Y) -> answer:second(answer:identity(Y), 44157).
-spec typed8(integer(), integer()) -> integer().
typed8(X, Y) -> answer:second(answer:identity(Y), 44157).
plain9(X, Y) -> answer:first(answer:identity(answer:second(answer:identity(X), Y)), answer:first(answer:identity(Y), X)).
-spec typed9(integer(), integer()) -> integer().
typed9(X, Y) -> answer:first(answer:identity(answer:second(answer:identity(X), Y)), answer:first(answer:identity(Y), X)).
plain10(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(answer:second(answer:identity(X), X)), answer:first(answer:identity(X), Y))), answer:second(answer:identity(answer:first(answer:identity(8116), Y)), answer:second(answer:identity(Y), Y))).
-spec typed10(integer(), integer()) -> integer().
typed10(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(answer:second(answer:identity(X), X)), answer:first(answer:identity(X), Y))), answer:second(answer:identity(answer:first(answer:identity(8116), Y)), answer:second(answer:identity(Y), Y))).
plain11(X, Y) -> answer:first(answer:identity(answer:second(answer:identity(answer:first(answer:identity(answer:first(answer:identity(-17366), Y)), answer:second(answer:identity(X), Y))), answer:first(answer:identity(answer:second(answer:identity(Y), X)), answer:second(answer:identity(X), X)))), answer:second(answer:identity(answer:second(answer:identity(answer:first(answer:identity(X), Y)), answer:second(answer:identity(-38049), X))), answer:first(answer:identity(answer:second(answer:identity(Y), -7239)), answer:first(answer:identity(Y), Y)))).
-spec typed11(integer(), integer()) -> integer().
typed11(X, Y) -> answer:first(answer:identity(answer:second(answer:identity(answer:first(answer:identity(answer:first(answer:identity(-17366), Y)), answer:second(answer:identity(X), Y))), answer:first(answer:identity(answer:second(answer:identity(Y), X)), answer:second(answer:identity(X), X)))), answer:second(answer:identity(answer:second(answer:identity(answer:first(answer:identity(X), Y)), answer:second(answer:identity(-38049), X))), answer:first(answer:identity(answer:second(answer:identity(Y), -7239)), answer:first(answer:identity(Y), Y)))).
plain12(X, Y) -> answer:second(answer:identity(26266), -30133).
-spec typed12(integer(), integer()) -> integer().
typed12(X, Y) -> answer:second(answer:identity(26266), -30133).
plain13(X, Y) -> answer:second(answer:identity(answer:first(answer:identity(Y), X)), answer:second(answer:identity(Y), 55231)).
-spec typed13(integer(), integer()) -> integer().
typed13(X, Y) -> answer:second(answer:identity(answer:first(answer:identity(Y), X)), answer:second(answer:identity(Y), 55231)).
plain14(X, Y) -> answer:second(answer:identity(answer:first(answer:identity(answer:first(answer:identity(Y), Y)), answer:second(answer:identity(Y), Y))), answer:first(answer:identity(answer:second(answer:identity(Y), X)), answer:second(answer:identity(-7252), -97563))).
-spec typed14(integer(), integer()) -> integer().
typed14(X, Y) -> answer:second(answer:identity(answer:first(answer:identity(answer:first(answer:identity(Y), Y)), answer:second(answer:identity(Y), Y))), answer:first(answer:identity(answer:second(answer:identity(Y), X)), answer:second(answer:identity(-7252), -97563))).
plain15(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(answer:first(answer:identity(answer:second(answer:identity(50573), Y)), answer:first(answer:identity(X), 70336))), answer:first(answer:identity(answer:first(answer:identity(-19846), X)), answer:second(answer:identity(36173), 98947)))), answer:first(answer:identity(answer:first(answer:identity(answer:first(answer:identity(64379), X)), answer:first(answer:identity(95912), 83400))), answer:second(answer:identity(answer:second(answer:identity(-83842), X)), answer:second(answer:identity(Y), X)))).
-spec typed15(integer(), integer()) -> integer().
typed15(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(answer:first(answer:identity(answer:second(answer:identity(50573), Y)), answer:first(answer:identity(X), 70336))), answer:first(answer:identity(answer:first(answer:identity(-19846), X)), answer:second(answer:identity(36173), 98947)))), answer:first(answer:identity(answer:first(answer:identity(answer:first(answer:identity(64379), X)), answer:first(answer:identity(95912), 83400))), answer:second(answer:identity(answer:second(answer:identity(-83842), X)), answer:second(answer:identity(Y), X)))).
plain16(X, Y) -> answer:second(answer:identity(X), 10583).
-spec typed16(integer(), integer()) -> integer().
typed16(X, Y) -> answer:second(answer:identity(X), 10583).
plain17(X, Y) -> answer:first(answer:identity(answer:first(answer:identity(Y), X)), answer:second(answer:identity(X), -21532)).
-spec typed17(integer(), integer()) -> integer().
typed17(X, Y) -> answer:first(answer:identity(answer:first(answer:identity(Y), X)), answer:second(answer:identity(X), -21532)).
plain18(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(answer:second(answer:identity(X), 80456)), answer:second(answer:identity(X), X))), answer:first(answer:identity(answer:second(answer:identity(Y), X)), answer:second(answer:identity(Y), -91601))).
-spec typed18(integer(), integer()) -> integer().
typed18(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(answer:second(answer:identity(X), 80456)), answer:second(answer:identity(X), X))), answer:first(answer:identity(answer:second(answer:identity(Y), X)), answer:second(answer:identity(Y), -91601))).
plain19(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(answer:second(answer:identity(answer:first(answer:identity(Y), 72791)), answer:second(answer:identity(X), Y))), answer:second(answer:identity(answer:second(answer:identity(X), X)), answer:first(answer:identity(Y), Y)))), answer:first(answer:identity(answer:first(answer:identity(answer:first(answer:identity(Y), -95960)), answer:second(answer:identity(X), -40803))), answer:second(answer:identity(answer:first(answer:identity(Y), -43303)), answer:second(answer:identity(Y), 25038)))).
-spec typed19(integer(), integer()) -> integer().
typed19(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(answer:second(answer:identity(answer:first(answer:identity(Y), 72791)), answer:second(answer:identity(X), Y))), answer:second(answer:identity(answer:second(answer:identity(X), X)), answer:first(answer:identity(Y), Y)))), answer:first(answer:identity(answer:first(answer:identity(answer:first(answer:identity(Y), -95960)), answer:second(answer:identity(X), -40803))), answer:second(answer:identity(answer:first(answer:identity(Y), -43303)), answer:second(answer:identity(Y), 25038)))).
plain20(X, Y) -> answer:first(answer:identity(50292), X).
-spec typed20(integer(), integer()) -> integer().
typed20(X, Y) -> answer:first(answer:identity(50292), X).
plain21(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(Y), 74617)), answer:first(answer:identity(-42938), Y)).
-spec typed21(integer(), integer()) -> integer().
typed21(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(Y), 74617)), answer:first(answer:identity(-42938), Y)).
plain22(X, Y) -> answer:first(answer:identity(answer:second(answer:identity(answer:first(answer:identity(179), 69201)), answer:first(answer:identity(X), Y))), answer:first(answer:identity(answer:first(answer:identity(Y), Y)), answer:first(answer:identity(Y), Y))).
-spec typed22(integer(), integer()) -> integer().
typed22(X, Y) -> answer:first(answer:identity(answer:second(answer:identity(answer:first(answer:identity(179), 69201)), answer:first(answer:identity(X), Y))), answer:first(answer:identity(answer:first(answer:identity(Y), Y)), answer:first(answer:identity(Y), Y))).
plain23(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(answer:second(answer:identity(answer:first(answer:identity(Y), -39965)), answer:first(answer:identity(62018), X))), answer:second(answer:identity(answer:second(answer:identity(86941), Y)), answer:first(answer:identity(Y), X)))), answer:first(answer:identity(answer:second(answer:identity(answer:second(answer:identity(X), -21126)), answer:second(answer:identity(Y), -2202))), answer:second(answer:identity(answer:first(answer:identity(-16842), Y)), answer:second(answer:identity(X), Y)))).
-spec typed23(integer(), integer()) -> integer().
typed23(X, Y) -> answer:second(answer:identity(answer:second(answer:identity(answer:second(answer:identity(answer:first(answer:identity(Y), -39965)), answer:first(answer:identity(62018), X))), answer:second(answer:identity(answer:second(answer:identity(86941), Y)), answer:first(answer:identity(Y), X)))), answer:first(answer:identity(answer:second(answer:identity(answer:second(answer:identity(X), -21126)), answer:second(answer:identity(Y), -2202))), answer:second(answer:identity(answer:first(answer:identity(-16842), Y)), answer:second(answer:identity(X), Y)))).
