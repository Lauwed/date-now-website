<script lang="ts">
	let { children, customClass = '', tag = 'div', centered = false } = $props();
</script>

<svelte:element this={tag} class={`card ${customClass} ${centered ? 'card--centered' : ''}`}>
	{@render children()}
</svelte:element>

<style lang="scss">
	@use './../styles/mixins.scss' as *;

	.card {
		@include glass(var(--blur, 75px), var(--background-opacity, 0.37));
		padding: 20px;

		&--centered {
			display: flex;
			justify-content: center;
			align-items: center;
			flex-direction: column;
			width: 100%;
			height: 100%;
			flex: 1;
		}

		& :global {
			h1,
			h2,
			h3,
			h4,
			h5,
			h6 {
				&:nth-child(1) {
					margin-top: 0;
				}
			}
		}
	}
</style>
