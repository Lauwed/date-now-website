<script lang="ts">
	let { children, customClass = '' } = $props();
</script>

<article class={`article ${customClass}`}>
	{@render children()}
</article>

<style lang="scss">
	@use './../styles/mixins.scss' as *;
	@use './../styles/variables.scss' as *;

	.article :global {
		@include glass(var(--blur, 75px), var(--background-opacity, 0.37));
		padding: 20px;

		hr {
			border: 0;
			background: $black;
			height: 2px;
			border-radius: 24px;
			opacity: 0.15;
		}

		ul {
			margin: revert;
			display: flex;
			flex-direction: column;
			gap: 4px;

			li {
				&::before {
					content: '->';
					display: inline-block;
					margin-right: 8px;
					font-family: 'Jetbrains Mono', monospace;
				}
			}
		}
	}
</style>
